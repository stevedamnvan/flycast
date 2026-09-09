// SPDX-License-Identifier: GPL-2.0-or-later
#include "harness.h"
#include "ta_provenance.h"
#include "capture_transition.h"
#include "remake_scene.h"
#include "rend/neural/pvr_scene_capture.h"
#include "rend/neural/pvr_material_capture.h"
#include "rend/neural/source_observation.h"
#include "rend/neural/source_sq_scope.h"
#include "rend/neural/source_read_link.h"
#include "rend/neural/source_transform.h"
#include "rend/neural/source_arithmetic.h"
#include <chrono>
#include <fstream>
#include "json/json.hpp"
#include "hw/pvr/ta_ctx.h"
#include "rend/neural/instrumentation.h"
#include "rend/neural/dlss5_hook.h"
#include "rend/neural/evidence_marker.h"
#include "rend/neural/live_status.h"
#include "rend/neural/motion_reference.h"
#include "rend/neural/neural_stage.h"
#include "rend/neural/presentation_cadence.h"
#include "rend/neural/quality_capture.h"
#include "rend/neural/quality_profile.h"
#include "rend/neural/external_control.h"
#include "rend/neural/producer_identity.h"
#include "rend/neural/remake_neural_input.h"
#include "rend/neural/remake_input_replay.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <memory>
#include <string>

namespace neuraltest {
namespace {

using namespace flycast::rend::neural;

struct Suite {
	int passed = 0;
	int failed = 0;

	void Expect(bool condition, const std::string& name)
	{
		if (condition)
		{
			++passed;
			std::cout << "PASS " << name << '\n';
		}
		else
		{
			++failed;
			std::cerr << "FAIL " << name << '\n';
		}
	}
};

DrawRecord BaseDraw(std::uint16_t ordinal = 4)
{
	DrawRecord draw{};
	draw.list = 0;
	draw.pass = 1;
	draw.stateSig = 0x12345678;
	draw.texId = 27;
	draw.firstVertex = 9;
	draw.vertexCount = 6;
	draw.firstIndex = 12;
	draw.indexCount = 6;
	draw.stripCount = 1;
	draw.uvSig = 0xabc;
	draw.topologySig = 0xdef;
	draw.textureGeneration = 3;
	draw.paletteGeneration = 7;
	draw.zMin = .2f;
	draw.zMax = .7f;
	draw.bboxMin[0] = 10;
	draw.bboxMin[1] = 20;
	draw.bboxMax[0] = 90;
	draw.bboxMax[1] = 100;
	draw.ordinal = ordinal;
	return draw;
}

bool Near(float a, float b, float epsilon = 1e-4f)
{
	return std::abs(a - b) <= epsilon;
}

} // namespace

int RunSelfTests()
{
	Suite suite;
	{
		PvrDecodedPacket p;p.frame=7;p.game="T1401N";p.gitSha="fixture";p.sourceProducer={2,6,100};
		p.framebufferSize={640,480};p.viewport={2.f/640,0,0,0,0,-2.f/480,0,0,0,0,1,0,-1,1,0,1};
		const std::array<std::array<float,2>,4> points{{{-1,-1},{1,-1},{-1,1},{1,1}}};
		for(unsigned i=0;i<4;++i) {
			SourceTransform t;t.serial=i+1;t.pc=0x8c03a9ea;t.input={points[i][0],points[i][1],10,1};
			t.matrix={614.714447f,0,0,0,0,565.537241f,0,0,0,0,1,0,0,0,0,1};
			t.output={points[i][0]*t.matrix[0],points[i][1]*t.matrix[5],10,i==0?0.f:1.f};
			::Vertex v{};v.x=t.output[0]/10+320;v.y=t.output[1]/10+240;v.z=.95f/10;
			v.u=.25f*i;v.v=.5f;v.col[0]=17;v.col[3]=255;v.spc[0]=9;p.vertices.push_back(v);
			SourceVertexObservation source;source.copy.decodedVertex=i;
			std::memcpy(source.copy.after.data()+1,&v.x,3*sizeof(float));source.copy.before=source.copy.after;
			for(auto& xyz:source.copy.xyzTransforms)xyz=t;p.sourceVertices.push_back(source);
		}
		PvrCapturedDraw draw;draw.state.init();draw.ordinal=18;draw.state.count=3;
		p.draws.push_back(draw);p.indices={0,1,2};
		RemakeViewScene view;std::string error;
		suite.Expect(BuildRemakeViewScene(p,p.sourceProducer,7,view,error) && view.meshes.size()==1
			&& view.meshes[0].vertices.size()==3 && view.meshes[0].sourceDraw.ordinal==18,
			"live view converts witnessed triangle with source draw identity");
		if(!view.meshes.empty()) {
			const auto& v=view.meshes[0].vertices[0];
			suite.Expect(Near(v.position[0],-1)&&Near(v.position[1],1)&&Near(v.position[2],10)
				&&Near(v.normal[2],-1)&&v.transformW==0&&v.source.col[0]==17&&v.source.spc[0]==9&&v.source.v==.5f,
				"live view preserves zero W and original attributes with separate derived normal");
		}
		const auto reject=[&](const PvrDecodedPacket& bad,const char* name) {
			// Default remains strictly observed even when the opt-in estimate exists.
			RemakeViewScene previous;previous.frame=99;
			suite.Expect(!BuildRemakeViewScene(bad,p.sourceProducer,7,previous,error)&&previous.frame==99,name);
		};
		{
			auto expanded=p;const auto first=unsigned(expanded.vertices.size());
			for(unsigned i=0;i<3;++i){auto v=p.vertices[i];v.x+=24;expanded.vertices.push_back(v);}
			auto draw=expanded.draws[0];draw.ordinal++;draw.state.first=unsigned(expanded.indices.size());draw.state.count=3;
			expanded.indices.insert(expanded.indices.end(),{first,first+1,first+2});expanded.draws.push_back(draw);
			RemakeViewScene observed,estimated;
			suite.Expect(BuildRemakeViewScene(expanded,p.sourceProducer,7,observed,error)&&observed.meshes.size()==1,
				"default observed lane still omits untraced geometry");
			const bool ok=BuildRemakeViewScene(expanded,p.sourceProducer,7,estimated,error,true);
			suite.Expect(ok&&estimated.meshes.size()==2&&estimated.estimatedVertices==3,
				"opt-in projected estimate expands untraced current geometry");
			if(ok&&estimated.meshes.size()==2){const auto& v=estimated.meshes[1].vertices[0];
				suite.Expect(v.estimatedPosition&&v.transformSerial==0&&Near(v.source.x,float(v.position[0]/v.position[2]*estimated.focalX+320))
					&&Near(v.source.z,.95f/v.position[2]),"estimated vertex is explicitly untraced and reprojects to source");}
			expanded.sourceVertices.clear();
			suite.Expect(!BuildRemakeViewScene(expanded,p.sourceProducer,7,estimated,error,true),"projected estimate requires current observed calibration anchor");
		}
		auto q=p;q.sourceProducer.ordinal++;reject(q,"live view rejects stale producer without replacing output");
		q=p;q.frame++;reject(q,"live view rejects wrong frame");
		q=p;q.game="unknown";reject(q,"live view rejects unsupported title");
		q=p;q.viewport[5]*=-1;reject(q,"live view rejects flipped viewport");
		q=p;q.sourceVertices[0].copy.xyzTransforms[1].reset();reject(q,"live view rejects incomplete XYZ draw");
		q=p;q.sourceVertices.push_back(q.sourceVertices[0]);reject(q,"live view rejects duplicate source vertex authority");
		q=p;q.vertices[0].x+=1;reject(q,"live view rejects source copy mismatch");
		q=p;for(auto& t:q.sourceVertices[0].copy.xyzTransforms)t->output[0]*=-1;reject(q,"live view wrong projection sign fails");
		q=p;for(auto& t:q.sourceVertices[0].copy.xyzTransforms)t->matrix[0]*=2;reject(q,"live view wrong lens scale fails");
		q=p;for(auto& t:q.sourceVertices[0].copy.xyzTransforms)t->output[2]*=-1;reject(q,"live view wrong depth polarity fails");
		q=p;q.draws[0].list=2;reject(q,"live view excludes translucent authoritative geometry");
		q=p;q.draws[0].state.projMatrix=0;reject(q,"live view excludes unproved Naomi2 domain");
		q=p;q.indices[0]=1234;reject(q,"live view rejects invalid index");
		q=p;q.indices.assign(262144,0);q.draws[0].state.count=262144;q.draws.push_back(q.draws[0]);
		reject(q,"live view bounds aggregate index work across overlapping draws");
		q=p;q.indices={0,1,2,3,UINT32_MAX,0,0,1,2};q.draws[0].state.count=unsigned(q.indices.size());
		suite.Expect(BuildRemakeViewScene(q,p.sourceProducer,7,view,error)&&view.meshes[0].vertices.size()==9
			&&view.degenerateTriangles==1&&Near(view.meshes[0].vertices[3].normal[2],-1),
			"live view retains strip parity through restart and degenerate vertices");
		MaterialPixels pixels;pixels.format=DXGI_FORMAT_B8G8R8A8_UNORM;pixels.mips.push_back({1,1,{10,20,30,255}});
		std::vector<unsigned char> dds;
		suite.Expect(EncodeRemakeMaterialDds(pixels,dds,error)&&remake::ValidSourceDdsBytes(dds)
			&&dds[148]==30&&dds[149]==20&&dds[150]==10,"live texture encoding retains BGRA to RGBA channel contract");
		auto badPixels=pixels;badPixels.mips[0].bytes.pop_back();const auto originalDds=dds;
		suite.Expect(!EncodeRemakeMaterialDds(badPixels,dds,error)&&dds==originalDds,"live texture malformed row rejects atomically");
		badPixels=pixels;badPixels.format=DXGI_FORMAT_A8_UNORM;
		suite.Expect(!EncodeRemakeMaterialDds(badPixels,dds,error),"live texture palette is not guessed");
		remake::Packet packet;
		const RemakeTextureReader reader=[&](const PvrCapturedDraw& draw,std::vector<unsigned char>& bytes,std::string&){
			if(draw.ordinal!=18)return false;bytes=dds;return true;};
		suite.Expect(BuildRemakeViewPacket(view,reader,packet,error)&&packet.meshes.size()==1
			&&packet.meshes[0].material->sourceDdsBytes==dds&&packet.producer.ordinal==p.sourceProducer.ordinal,
			"live geometry and owned texture form the shared Remix packet");
		const auto wirePath=std::filesystem::temp_directory_path()/("flycast-view-wire-"+std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count())+".bin");
		remake::Packet decoded;
		const bool wrote=WriteRemakeViewPacket(wirePath,packet,error);
		suite.Expect(wrote&&ReadRemakeViewPacket(wirePath,decoded,error)&&decoded.frame==packet.frame
			&&decoded.producer.cycle==packet.producer.cycle&&decoded.meshes[0].vertices.size()==9
			&&decoded.meshes[0].material->sourceDdsBytes==dds&&decoded.omissions==packet.omissions
			&&decoded.meshes[0].vertices[0].publicColor==packet.meshes[0].vertices[0].publicColor,
			"live view wire retains source identity geometry materials and limitations");
		suite.Expect(!WriteRemakeViewPacket(wirePath,packet,error),"live view wire never overwrites an existing output");
		if(wrote) {
			{std::fstream file(wirePath,std::ios::in|std::ios::out|std::ios::binary);char bad=0;file.write(&bad,1);}
			decoded.frame=99;
			suite.Expect(!ReadRemakeViewPacket(wirePath,decoded,error)&&decoded.frame==99,"live view wire wrong schema rejects atomically");
			std::filesystem::remove(wirePath);WriteRemakeViewPacket(wirePath,packet,error);
			{std::ofstream file(wirePath,std::ios::app|std::ios::binary);file.put(0);}
			suite.Expect(!ReadRemakeViewPacket(wirePath,decoded,error),"live view wire trailing byte rejects");
			std::filesystem::remove(wirePath);WriteRemakeViewPacket(wirePath,packet,error);
			std::filesystem::resize_file(wirePath,20);
			suite.Expect(!ReadRemakeViewPacket(wirePath,decoded,error),"live view wire truncated identity rejects");
			std::filesystem::remove(wirePath);
		}
		auto next=packet;next.frame++;next.producer.ordinal++;next.producer.cycle++;
		auto relabeled=packet;relabeled.frame+=17;relabeled.sourceGitSha="new-build";
		for(auto& mesh:relabeled.meshes)mesh.frame=relabeled.frame;
		suite.Expect(SameRemakeReplayScene(packet,relabeled,error),"locked replay permits renderer counter and build label differences only");
		relabeled.producer.cycle++;
		suite.Expect(!SameRemakeReplayScene(packet,relabeled,error),"locked replay rejects game clock mismatch");
		relabeled=packet;relabeled.meshes[0].vertices[0].position.x+=1;
		suite.Expect(!SameRemakeReplayScene(packet,relabeled,error),"locked replay rejects changed geometry");
		relabeled=packet;relabeled.meshes[0].material->sourceDdsBytes.back()^=1;
		suite.Expect(!SameRemakeReplayScene(packet,relabeled,error),"locked replay rejects changed texture bytes");
		{
			auto root=wirePath;root += ".locked";
			if(std::filesystem::create_directory(root)) {
				const auto folder=root/"frame-test";std::filesystem::create_directory(folder);
				WriteRemakeViewPacket(folder/"remake-view.bin",packet,error);
				RemakeReturnedImage image;image.frame=packet.frame;image.producer=packet.producer;
				image.width=640;image.height=480;image.bgra.assign(640*480*4,73);image.projectionDepth.assign(640*480,.75f);
				image.nearPlane=packet.camera.nearPlane;image.farPlane=packet.camera.farPlane;
				RemakeNeuralInput input;BuildRemakeNeuralInput(image,image.frame,image.producer,input);
				auto digest=[](const void* data,size_t size){auto p=static_cast<const unsigned char*>(data);std::uint64_t h=14695981039346656037ull;for(size_t i=0;i<size;++i){h^=p[i];h*=1099511628211ull;}return h;};
				auto hex=[](std::uint64_t h){std::ostringstream out;out<<std::uppercase<<std::hex<<std::setw(16)<<std::setfill('0')<<h;return out.str();};
				std::ostringstream wire(std::ios::binary);SerializeRemakeViewPacket(wire,packet,error);const auto bytes=wire.str();
				{std::ofstream f(folder/"remake-return.bgra",std::ios::binary);f.write(reinterpret_cast<const char*>(image.bgra.data()),image.bgra.size());}
				{std::ofstream f(folder/"remake-return-depth.f32",std::ios::binary);f.write(reinterpret_cast<const char*>(image.projectionDepth.data()),image.projectionDepth.size()*4);}
				nlohmann::json receipt={{"frame",packet.frame},{"source_digest",digest(bytes.data(),bytes.size())},{"sequence",1},{"depth_values",640*480},{"pixel_bytes",640*480*4}};
				{std::ofstream f(folder/"remake-return.json");f<<receipt;}
				nlohmann::json manifest={{"frame_id",packet.frame},{"git_sha",packet.sourceGitSha},{"remake_input","returned-scene-reset-only-inverted-projection-experiment"},
					{"producer_identity",{{"epoch",packet.producer.epoch},{"ordinal",packet.producer.ordinal},{"cycle",packet.producer.cycle}}},
					{"contract_hashes",{{"color_fnv64",hex(digest(input.rgba.data(),input.rgba.size()))},{"depth_fnv64",hex(digest(input.invertedDepth.data(),input.invertedDepth.size()*4))}}}};
				{std::ofstream f(folder/"manifest.json");f<<manifest;}
				RemakeReturnedImage replay;replay.frame=999;std::uint64_t origin=0;
				suite.Expect(ReadLockedRemakeInput(root,packet,replay,origin,error)&&replay.bgra==image.bgra&&origin==packet.frame,"locked replay loads exact source-qualified pixels");
				struct GroupedNumbers:std::numpunct<char>{std::string do_grouping() const override{return "\3";} char do_thousands_sep() const override{return ',';}};
				const auto priorLocale=std::locale::global(std::locale(std::locale::classic(),new GroupedNumbers));
				const bool groupedResult=ReadLockedRemakeInput(root,packet,replay,origin,error);
				std::locale::global(priorLocale);
				suite.Expect(groupedResult,"locked replay hashes remain portable under grouped user locale");
				auto changed=packet;changed.producer.cycle++;
				suite.Expect(!ReadLockedRemakeInput(root,changed,replay,origin,error)&&replay.bgra==image.bgra,"locked replay missing producer preserves caller output");
				{std::fstream f(folder/"remake-return.bgra",std::ios::binary|std::ios::in|std::ios::out);f.put(42);}
				suite.Expect(!ReadLockedRemakeInput(root,packet,replay,origin,error)&&error=="locked-replay-source-input-hash-mismatch","locked replay rejects changed color bytes");
				std::filesystem::resize_file(folder/"remake-return-depth.f32",4);
				suite.Expect(!ReadLockedRemakeInput(root,packet,replay,origin,error)&&error=="locked-replay-file-extent","locked replay rejects truncated depth");
				for(const auto& f:std::filesystem::directory_iterator(folder))std::filesystem::remove(f.path());
				std::filesystem::remove(folder);std::filesystem::remove(root);
			}else suite.Expect(false,"locked replay fixture must own a new directory");
		}
		suite.Expect(remake::DiagnosticContinuation(packet,next),"live packet accepts consecutive producer stamp");
		{
			auto skipped=next;skipped.frame+=3;skipped.producer.ordinal+=3;skipped.producer.cycle+=3;
			suite.Expect(remake::AsyncSourceContinuation(packet,skipped)&&!remake::DiagnosticContinuation(packet,skipped),"async dropped source accepted only by explicit non-temporal policy");
			auto wrong=skipped;++wrong.producer.epoch;
			suite.Expect(!remake::AsyncSourceContinuation(packet,wrong),"async source rejects different epoch");
			wrong=skipped;wrong.producer.ordinal=packet.producer.ordinal;
			suite.Expect(!remake::AsyncSourceContinuation(packet,wrong),"async source rejects repeated producer");
			wrong=skipped;wrong.frame=packet.frame;
			suite.Expect(!remake::AsyncSourceContinuation(packet,wrong),"async source rejects repeated renderer frame");
			wrong=skipped;wrong.game="other";
			suite.Expect(!remake::AsyncSourceContinuation(packet,wrong),"async source rejects changed game");
			wrong=skipped;wrong.sourceGitSha="other";
			suite.Expect(!remake::AsyncSourceContinuation(packet,wrong),"async source rejects changed source build");
			wrong=skipped;wrong.diagnosticOrigin->x+=1;
			suite.Expect(!remake::AsyncSourceContinuation(packet,wrong),"async source rejects changed coordinate origin");
		}
		next.producer.epoch++;
		suite.Expect(!remake::DiagnosticContinuation(packet,next),"live packet rejects reset epoch continuity");
		{
			RemakeLiveChannel consumer,publisher,duplicate;RemakeChannelReceipt sent,received;
			const auto token="test-"+std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
			suite.Expect(!publisher.OpenPublisher(token,error),"live channel missing consumer does not wait");
			suite.Expect(!consumer.CreateConsumer("../invalid",error),"live channel rejects path-like token");
			const bool created=consumer.CreateConsumer(token,error),opened=publisher.OpenPublisher(token,error);
			suite.Expect(created&&opened,"live channel opens one consumer and publisher");
			suite.Expect(!duplicate.CreateConsumer(token,error)&&!duplicate.OpenPublisher(token,error),"live channel rejects duplicate owner and publisher");
			const auto advance=[](remake::Packet p){++p.frame;++p.producer.ordinal;++p.producer.cycle;for(auto& mesh:p.meshes)mesh.frame=p.frame;return p;};
			{
				RemakeLiveChannel returnConsumer,returnPublisher;RemakeChannelReceipt receipt,first,secondReceipt;
				const auto returnToken=token+"-credit";
				bool ready=returnConsumer.CreateConsumer(returnToken,error)&&returnPublisher.OpenPublisher(returnToken,error);
				auto b=advance(packet),c=advance(b);remake::Packet owned;
				ready=ready&&returnPublisher.PublishForReturn(packet,receipt,error)==RemakeChannelResult::Published
					&&returnConsumer.Receive(owned,first,error)==RemakeChannelResult::Received
					&&returnPublisher.PublishForReturn(b,receipt,error)==RemakeChannelResult::Published
					&&returnConsumer.Receive(owned,secondReceipt,error)==RemakeChannelResult::Received;
				suite.Expect(ready,"return-aware source slots consumed with two replies outstanding");
				suite.Expect(returnPublisher.PublishForReturn(c,receipt,error)==RemakeChannelResult::Busy
					&&error=="channel-return-credit-busy","delayed replies prevent source ledger overwrite despite free transport slots");
				RemakeReturnedImage delayed;delayed.source=first;delayed.frame=packet.frame;delayed.producer=packet.producer;
				delayed.width=640;delayed.height=480;delayed.bgra.assign(640*480*4,73);
				RemakeReturnedImage accepted;
				suite.Expect(returnConsumer.ReturnImage(delayed,error)==RemakeChannelResult::Published
					&&returnPublisher.ReceiveImage(accepted,error)==RemakeChannelResult::Received
					&&accepted.frame==packet.frame,"delayed first reply retains original frame after credit rejection");
				suite.Expect(returnPublisher.PublishForReturn(c,receipt,error)==RemakeChannelResult::Published
					&&receipt.sequence==3,"returned image releases exactly one source credit without advancing on busy");
				suite.Expect(returnPublisher.ExpireReturns(c.frame,c.producer,1)==0,"source age at limit remains owned");
				suite.Expect(returnPublisher.ExpireReturns(c.frame+1,c.producer,1)==1,"source older than age bound expires without waiting");
				delayed.source=secondReceipt;delayed.frame=b.frame;delayed.producer=b.producer;
				suite.Expect(returnConsumer.ReturnImage(delayed,error)==RemakeChannelResult::Published
					&&returnPublisher.ReceiveImage(accepted,error)==RemakeChannelResult::Invalid
					&&accepted.frame==packet.frame,"expired late reply rejects without changing owned output");
				auto epoch=c.producer;++epoch.epoch;
				suite.Expect(returnPublisher.ExpireReturns(c.frame,epoch,10)==1,"epoch reset expires all remaining source ownership");
			}
			auto second=advance(packet),third=advance(second);
			remake::Packet receivedPacket;receivedPacket.frame=99;
			suite.Expect(consumer.Receive(receivedPacket,received,error)==RemakeChannelResult::Empty&&receivedPacket.frame==99,
				"live channel empty receive preserves caller output");
			suite.Expect(publisher.Publish(packet,sent,error)==RemakeChannelResult::Published&&sent.sequence==1,
				"live channel publishes owned packet without files");
			const auto firstReceipt=sent;
			suite.Expect(publisher.Publish(second,sent,error)==RemakeChannelResult::Published,
				"live channel queues second bounded slot");
			suite.Expect(publisher.Publish(third,sent,error)==RemakeChannelResult::Busy,
				"live channel full ring skips instead of waiting");
			suite.Expect(consumer.Receive(receivedPacket,received,error)==RemakeChannelResult::Received
				&&receivedPacket.frame==packet.frame&&receivedPacket.meshes[0].material->sourceDdsBytes==packet.meshes[0].material->sourceDdsBytes
				&&received.digest==firstReceipt.digest&&received.bytes==firstReceipt.bytes,
				"live channel receives exact owned payload and matching receipt");
			RemakeReturnedImage image;image.source=received;image.frame=receivedPacket.frame;image.producer=receivedPacket.producer;
			image.width=640;image.height=480;image.bgra.assign(640*480*4,73);
			RemakeReturnedImage returned;returned.frame=999;
			suite.Expect(publisher.ReceiveImage(returned,error)==RemakeChannelResult::Empty&&returned.frame==999,"return empty preserves output");
			auto wrong=image;wrong.frame++;
			suite.Expect(consumer.ReturnImage(wrong,error)==RemakeChannelResult::Invalid,"return rejects wrong frame");
			wrong=image;wrong.source.digest++;
			suite.Expect(consumer.ReturnImage(wrong,error)==RemakeChannelResult::Invalid,"return rejects wrong source digest");
			wrong=image;wrong.bgra.pop_back();
			suite.Expect(consumer.ReturnImage(wrong,error)==RemakeChannelResult::Invalid,"return rejects truncated pixels");
			wrong=image;wrong.projectionDepth.assign(10,.5f);wrong.nearPlane=packet.camera.nearPlane;wrong.farPlane=packet.camera.farPlane;
			suite.Expect(consumer.ReturnImage(wrong,error)==RemakeChannelResult::Invalid,"return rejects truncated depth before color publication");
			suite.Expect(publisher.ReceiveImage(returned,error)==RemakeChannelResult::Empty,"failed depth cannot publish color alone");
			suite.Expect(consumer.ReturnImage(image,error)==RemakeChannelResult::Published
				&&publisher.ReceiveImage(returned,error)==RemakeChannelResult::Received&&returned.bgra==image.bgra
				&&returned.frame==image.frame&&returned.source.digest==image.source.digest,"return exact owned pixels and source receipt");
			suite.Expect(consumer.ReturnImage(image,error)==RemakeChannelResult::Invalid,"return rejects duplicate image");
			suite.Expect(publisher.Publish(third,sent,error)==RemakeChannelResult::Published&&sent.sequence==3,
				"live channel busy attempt does not advance publication sequence");
			suite.Expect(consumer.Receive(receivedPacket,received,error)==RemakeChannelResult::Received&&receivedPacket.frame==second.frame
				&&consumer.Receive(receivedPacket,received,error)==RemakeChannelResult::Received&&receivedPacket.frame==third.frame,
				"live channel preserves FIFO after lower-slot reuse");
			image.source=received;image.frame=receivedPacket.frame;image.producer=receivedPacket.producer;
			image.projectionDepth.assign(640*480,.75f);image.projectionDepth[0]=0;image.projectionDepth[1]=1;
			image.nearPlane=receivedPacket.camera.nearPlane;image.farPlane=receivedPacket.camera.farPlane;
			RemakeNeuralInput converted;
			wrong=image;std::fill(wrong.bgra.begin(),wrong.bgra.end(),0);std::fill(wrong.projectionDepth.begin(),wrong.projectionDepth.end(),0);
			suite.Expect(!BuildRemakeNeuralInput(wrong,image.frame,image.producer,converted),"remake input rejects wholly empty color and depth readback");
			image.bgra[0]=11;image.bgra[1]=22;image.bgra[2]=33;image.bgra[3]=44;
			suite.Expect(BuildRemakeNeuralInput(image,image.frame,image.producer,converted)
				&&converted.rgba[0]==33&&converted.rgba[1]==22&&converted.rgba[2]==11&&converted.rgba[3]==44
				&&converted.invertedDepth[0]==1&&converted.invertedDepth[1]==0&&converted.invertedDepth[2]==.25f,
				"remake input swaps BGRA only and inverts exact projection endpoints");
			const auto kept=converted.invertedDepth;
			suite.Expect(!BuildRemakeNeuralInput(image,image.frame+1,image.producer,converted)
				&&converted.invertedDepth==kept,"remake input rejects wrong frame atomically");
			auto wrongProducer=image.producer;wrongProducer.ordinal++;
			suite.Expect(!BuildRemakeNeuralInput(image,image.frame,wrongProducer,converted),"remake input rejects wrong producer");
			wrong=image;wrong.projectionDepth.clear();
			suite.Expect(!BuildRemakeNeuralInput(wrong,image.frame,image.producer,converted),"remake input refuses color-only guidance");
			wrong=image;wrong.projectionDepth[2]=std::numeric_limits<float>::quiet_NaN();
			suite.Expect(!BuildRemakeNeuralInput(wrong,image.frame,image.producer,converted)
				&&converted.invertedDepth==kept,"remake input rejects NaN without partial conversion");
			wrong=image;wrong.nearPlane+=.1f;
			suite.Expect(consumer.ReturnImage(wrong,error)==RemakeChannelResult::Invalid,"return rejects depth from wrong projection");
			wrong=image;wrong.projectionDepth[0]=std::numeric_limits<float>::quiet_NaN();
			suite.Expect(consumer.ReturnImage(wrong,error)==RemakeChannelResult::Invalid,"return rejects nonfinite depth");
			wrong=image;wrong.projectionDepth[0]=1.01f;
			suite.Expect(consumer.ReturnImage(wrong,error)==RemakeChannelResult::Invalid,"return rejects out-of-range projection depth");
			suite.Expect(consumer.ReturnImage(image,error)==RemakeChannelResult::Published,"return publishes newer source image");
			suite.Expect(publisher.Publish(third,sent,error)==RemakeChannelResult::Invalid,"live channel rejects duplicate source frame");
			auto fourth=advance(third),bad=fourth;bad.meshes[0].vertices[0].normal.reset();
			suite.Expect(publisher.Publish(bad,sent,error)==RemakeChannelResult::Invalid
				&&publisher.Publish(fourth,sent,error)==RemakeChannelResult::Published&&sent.sequence==4,
				"live channel failed serialization releases slot without advancing sequence");
			suite.Expect(consumer.Receive(receivedPacket,received,error)==RemakeChannelResult::Received,"return next source available");
			image.source=received;image.frame=receivedPacket.frame;image.producer=receivedPacket.producer;
			suite.Expect(consumer.ReturnImage(image,error)==RemakeChannelResult::Busy,"return full slot does not wait");
			suite.Expect(publisher.ReceiveImage(returned,error)==RemakeChannelResult::Received&&returned.frame==third.frame
				&&consumer.ReturnImage(image,error)==RemakeChannelResult::Published,"return busy retry preserves source ownership");
			consumer.Close();
			suite.Expect(publisher.ReceiveImage(returned,error)==RemakeChannelResult::Received&&returned.frame==fourth.frame
				&&returned.projectionDepth==image.projectionDepth&&returned.nearPlane==image.nearPlane&&returned.farPlane==image.farPlane,
				"return completed image survives orderly consumer close");
			suite.Expect(publisher.Publish(advance(fourth),sent,error)==RemakeChannelResult::Closed,
				"live channel consumer shutdown leaves producer in native fallback");
		}
	}
	{
		std::string args;
		ExternalControlValues values{200, 200, 75, 75, 2, false, true};
		suite.Expect(BuildExternalControlArguments(values, args)
			&& args == " --apply --overall 2.00 --structure 2.00 --global-tone 0.75 --local-tone 0.75 --style cinematic --auto-mask off --ui-correction on",
			"consumer controls convert percentages to the documented companion units");
		values.structure = 201;
		suite.Expect(!BuildExternalControlArguments(values, args) && args.empty(),
			"consumer controls reject out-of-range values without a command");
		values.structure = 0;
		values.style = 3;
		suite.Expect(!BuildExternalControlArguments(values, args),
			"consumer controls reject undocumented styles");
		suite.Expect(!ApplyExternalControls("", "", {}).success
			&& !ApplyExternalControls("bad\"path", "test.ini", {}).success,
			"consumer control launch rejects missing or quoted paths before execution");
	}
	{
		QualityCaptureWriter capture;
		SourceObservationBatch observations;
		{
			SourceSqScope sq(0x8c001000,0xe0000020);
			const auto invocation=currentSourceSq;
			suite.Expect(invocation.serial&&invocation.pc==0x8c001000&&invocation.address==0xe0000020,
				"source SQ invocation retains observed PC and address");
			{SourceSqScope nested(0x8c002000,0xe0000040);
			 suite.Expect(!currentSourceSq.serial,"source SQ nested invocation not falsely attributed");
			 {SourceSqScope deeper(0x8c003000,0xe0000060);
			 suite.Expect(!currentSourceSq.serial,"source SQ third nesting cannot restore attribution");}}
			suite.Expect(currentSourceSq.serial==invocation.serial,"source SQ nested exit restores outer scope");
		}
		suite.Expect(!currentSourceSq.serial,"source SQ exit clears invocation");
		const auto savedSerial=sourceSqSerial;sourceSqSerial=UINT64_MAX;
		ObserveSourceSqStore(0xe0000004,0x8c000200,4,0x12345678);
		ObserveSourceSqStore(0xe0000024,0x8c000204,4,0xabcdef00);
		suite.Expect(sourceSqWriters[4].value==0x78&&sourceSqWriters[7].value==0x12
			&&sourceSqWriters[36].pc==0x8c000204,"source SQ executed stores preserve byte lanes and queue selection");
		{SourceSqScope consumed(0x8c001000,0xe0000000);}
		suite.Expect(!sourceSqWriters[4].pc&&sourceSqWriters[36].pc==0x8c000204,
			"source SQ consumed queue cannot retain stale writer authority");
		sourceSqWriters={};
		ObserveSourceSqStore(0xe0000024,0x8c000204,4,0xabcdef00);
		ResetSourceSqWriters();
		{SourceSqScope resetSubmission(0x8c001000,0xe0000020);
		 suite.Expect(!sourceSqWriters[36].pc,"source SQ reset invalidates deferred producer metadata");}
		ObserveSourceSqStore(0xe0000024,0x8c000204,4,0xabcdef00);
		InvalidateSourceSqWriters();
		suite.Expect(!sourceSqWriters[36].pc,"source SQ unsupported interpreter path invalidates writers");
		sourceRegisterReads[0]={0x8c001000,0x8c002000,0x12345678,true};
		ObserveSourceSqStore(0xe0000004,0x8c002006,4|(1<<8),0x12345678);
		suite.Expect(sourceSqWriters[4].ram==0x8c001000&&sourceSqWriters[7].ram==0x8c001003
			&&sourceSqWriters[4].readPc==0x8c002000,"source SQ direct register read retains byte-address lineage");
		ObserveSourceSqStore(0xe0000004,0x8c002006,4|(1<<8),0x12345679);
		suite.Expect(!sourceSqWriters[4].ram,"source SQ mismatched read value rejects RAM lineage");
		InvalidateSourceSqWriters();
		{SourceSqScope exhausted(0x8c001000,0xe0000020);
		 suite.Expect(!currentSourceSq.serial,"source SQ serial exhaustion rejects attribution");}
		sourceSqSerial=savedSerial;
		try {SourceSqScope unwinding(0x8c001000,0xe0000020);throw 1;}catch(int){}
		suite.Expect(!currentSourceSq.serial&&!sourceSqScopeActive,"source SQ exception unwinds attribution");
		ProducerIdentity identity{3,100,900};SourceCopyObservation observation;
		{
		 PvrDecodedPacket packet;packet.vertices.resize(3);packet.indices={0,1,2};
		 PvrCapturedDraw draw;draw.state.first=0;draw.state.count=3;packet.draws.push_back(draw);
		 SourceTransform transform;transform.serial=7;transform.pc=0x8c001000;
		 for(unsigned i=0;i<3;++i) {SourceVertexObservation source;source.copy.decodedVertex=i;
		  for(auto& xyz:source.copy.xyzTransforms)xyz=transform;packet.sourceVertices.push_back(source);}
		 auto coverage=MeasurePvrSourceCoverage(packet);
		 packet.sourceProducer={1,2,3};packet.frame=4;packet.game="fixture";
		 const auto witnessPath=std::filesystem::temp_directory_path()/("flycast-source-witness-"+std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count())+".json");
		 std::string witnessError;
		 const bool wrote=WritePvrSourceWitness(witnessPath,packet,witnessError);
		 nlohmann::json witness;if(wrote){std::ifstream input(witnessPath);input>>witness;}
		 suite.Expect(wrote&&witness["vertices"].size()==3&&witness["transforms"].size()==1
			&&witness["producer"][1]==2,"source witness retains frame identity and deduplicated transform bits");
		 std::filesystem::remove(witnessPath);
		 packet.sourceProducer={};
		 suite.Expect(!WritePvrSourceWitness(witnessPath,packet,witnessError),"source witness rejects missing producer identity");
		 suite.Expect(coverage.commonOriginVertices==3&&coverage.completeDraws==1,"source coverage requires every draw vertex");
		 packet.sourceVertices[1].copy.xyzTransforms[1]->serial=8;
		 coverage=MeasurePvrSourceCoverage(packet);
		 suite.Expect(coverage.completeVertices==3&&coverage.commonOriginVertices==2&&coverage.completeDraws==0&&coverage.partialDraws==1,
			"source coverage separates complete XYZ from common transform origin");
		}
		{
		 SourceArithmetic arithmetic;arithmetic.lhs=0x40000000;arithmetic.rhs=0x3f800000;
		 arithmetic.layout=1;arithmetic.result=0x40400000;
		 suite.Expect(VerifySourceArithmetic(arithmetic),"arithmetic witness verifies actual add bits");
		 arithmetic.result^=1;
		 suite.Expect(!VerifySourceArithmetic(arithmetic),"arithmetic witness rejects one-bit wrong result");
		 arithmetic.layout=4;arithmetic.rhs=0;arithmetic.result=0x7f800000;
		 suite.Expect(!VerifySourceArithmetic(arithmetic),"arithmetic witness rejects singular division");
		 ClearSourceArithmeticOrigins();SourceTransform transform;transform.serial=123;transform.output[0]=2.0f;
		 SeedSourceArithmeticOrigin(4,transform);
		 suite.Expect(ValidateSourceArithmeticRegister(4,0x40000000)&&sourceArithmeticLive==4,
			"block entry retains exact live register transform");
		 suite.Expect(!ValidateSourceArithmeticRegister(5,1)&&sourceArithmeticLive==3,
			"block entry rejects changed register bits independently");
		 BeginSourceArithmetic(0x8c001000,3|(5<<8)|(4<<16)|(255u<<24),0x40000000,0x40400000);
		 EndSourceArithmetic(0x40c00000);
		 suite.Expect(GetSourceArithmeticOrigin(5,0x40c00000)&&GetSourceArithmeticOrigin(5,0x40c00000)->serial==123,
			"verified multiply propagates actual transform identity");
		 transform.serial=124;transform.output[0]=1.0f;SeedSourceArithmeticOrigin(6,transform);
		 BeginSourceArithmetic(0x8c001002,1|(7<<8)|(5<<16)|(6u<<24),0x40c00000,0x3f800000);EndSourceArithmetic(0x40e00000);
		 suite.Expect(!GetSourceArithmeticOrigin(7,0x40e00000),"mixed transform arithmetic rejects ambiguous origin");
		 KillSourceArithmeticOrigin(5,1);
		 suite.Expect(!GetSourceArithmeticOrigin(5,0x40c00000),"overwritten register cannot revive same-value transform tag");
		 ClearSourceArithmeticOrigins();
		}
		ObserveSourceRamWrite(0x8c001000,0x8c002000,4,0x12345678);
		suite.Expect(SourceRamWriter(0xac001000,0x12345678)==0x8c002000,
			"RAM writer physical alias retains exact observed value");
		ObserveSourceRamWrite(0x8c001001,0x8c002002,1,0x56);
		suite.Expect(!SourceRamWriter(0x8c001000,0x12345678),"partial RAM write rejects old whole-word authority");
		ObserveSourceRamWrite(0x8c001000,0x8c002000,4,0x12345678);
		ObserveSourceRamWrite(0x8c011000,0x8c002004,4,0x12345678);
		suite.Expect(!SourceRamWriter(0x8c001000,0x12345678)
			&&SourceRamWriter(0x8c011000,0x12345678)==0x8c002004,"RAM writer collision cannot match another physical address");
		InvalidateSourceRamWrites();
		{
		 std::vector<shil_opcode> ops(3);
		 ops[0].op=shop_readm;ops[0].size=4;ops[0].rd=shil_param(reg_fr_1);
		 ops[1].op=shop_mov32;ops[1].rd=shil_param(reg_fr_2);
		 ops[2].op=shop_writem;ops[2].size=4;ops[2].rs2=shil_param(reg_fr_1);
		 suite.Expect(DirectSourceRead(ops,2)==0,"direct source read survives unrelated register write");
		 ops[1].rd=shil_param(reg_fr_1);
		 suite.Expect(DirectSourceRead(ops,2)<0,"direct source read rejects intervening overwrite");
		 ops[1].rd=shil_param(regv_fv_0);
		 suite.Expect(DirectSourceRead(ops,2)<0,"direct source read rejects overlapping vector write");
		 ops[1].rd=shil_param();ops[1].rd2=shil_param(reg_fr_1);
		 suite.Expect(DirectSourceRead(ops,2)<0,"direct source read rejects secondary destination write");
		 ops[1].rd2=shil_param();ops[1].op=shop_ifb;
		 suite.Expect(DirectSourceRead(ops,2)<0,"direct source read rejects interpreter boundary");
		 ops[0].op=shop_ftrv;ops[0].rd=shil_param(regv_fv_0);
		 ops[1].op=shop_mov32;ops[1].rd=shil_param(reg_fr_8);
		 suite.Expect(DirectSourceTransform(ops,2)==0,"direct transform store finds exact component definition");
		 ops[1].rd=shil_param(reg_fr_1);
		 suite.Expect(DirectSourceTransform(ops,2)<0,"direct transform store rejects intervening arithmetic or overwrite");
		 SourceTransform transform;transform.pc=0x8c001000;transform.output[3]=2.0f;
		 RetainSourceTransform(transform);const auto serial=sourceTransformSerial;
		 suite.Expect(FindSourceTransform(serial)&&FindSourceTransform(serial)->output[3]==2.0f,
			"executed transform lookup preserves nonunit W");
		 for(unsigned n=0;n<4096;++n)RetainSourceTransform(transform);
		 suite.Expect(!FindSourceTransform(serial),"expired transform serial cannot alias replacement ring record");
		 ObserveSourceRamWrite(0x8c001000,0x8c002000,4,0x12345678);
		 auto& writer=(*sourceRamWrites)[0x1000/4];
		 transform.serial=serial;writer.transform=serial;writer.ownedTransform=transform;
		 const auto owned=SourceRamTransform(0x8c001000,0x12345678);
		 ObserveSourceRamWrite(0x8c002000,0x8c003000,4,0x12345678);
		 suite.Expect(CarrySourceRamTransform(0x8c002000,0x8c003000,0x12345678,owned)
			&&SourceRamTransform(0x8c002000,0x12345678)->serial==serial,
			"direct RAM copy preserves owned transform through a second address");
		 suite.Expect(!CarrySourceRamTransform(0x8c002000,0x8c003002,0x12345678,owned)
			&&!CarrySourceRamTransform(0x8c002000,0x8c003000,0x12345679,owned),
			"RAM transform forwarding rejects wrong writer and value");
		 suite.Expect(owned&&owned->serial==serial&&!FindSourceTransform(serial),
			"RAM writer owns transform after observation ring eviction");
		 InvalidateSourceRamWrites();
		 suite.Expect(!SourceRamTransform(0x8c001000,0x12345678)&&owned&&owned->output[3]==2.0f,
			"RAM invalidation rejects new lookup without invalidating owned transform copy");
		}
		observation.generation=1;observation.writerPc=0x8c000100;observation.cycle=800;
		observations.BeginContext(12);
		suite.Expect(observations.Append(observation)&&!observations.Get(identity)
			&&observations.Seal(identity,1)&&observations.Get(identity),
			"source observation stamps only at producer queue boundary");
		observations.BeginContext(13);observations.Append(observation);
		suite.Expect(!observations.Seal({3,100,799},1)&&!observations.Get(identity),
			"source observation rejects copy newer than queue stamp");
		observations.Begin(identity);
		suite.Expect(observations.Append(observation)&&observations.Seal(1)&&observations.Get(identity),
			"source observation seals exact frame copy");
		suite.Expect(!observations.JoinVertex({4,100,900},0,observation.after.data(),7)
			&& !observations.JoinVertex(identity,32,observation.after.data(),7)
			&& observations.JoinVertex(identity,0,observation.after.data(),7)
			&& !observations.JoinVertex(identity,0,observation.after.data(),8)
			&& observations.Get(identity)->front().decodedVertex==7,
			"source vertex join requires exact frame offset and unique consumer");
		suite.Expect(!observations.Get({4,100,900})&&!observations.Get({3,101,900}),
			"source observation rejects reset and wrong frame");
		observations.Begin({3,101,1000});
		suite.Expect(!observations.Get(identity)&&!observations.Seal(1),"source observation reset and incomplete reject");
		observations.Begin(identity);observation.after[1]=1;
		suite.Expect(!observations.Append(observation)&&!observations.Seal(1),"source observation altered copy rejects");
		observations.Begin(identity);observation.after[1]=0;
		suite.Expect(observations.Append(observation)&&!observations.Append(observation)&&!observations.Seal(1),
			"source observation duplicate offset poisons batch");
		suite.Expect(capture.CapturedPvrSnapshot(1)==nullptr,"quality snapshot initially unavailable");
		capture.Configure("capture-a", 0, 2);
		suite.Expect(capture.CapturesCurrentFrame() && capture.ConsumeCaptureStart()
			&& !capture.ConsumeCaptureStart(),
			"quality capture emits one temporal reset at its first retained frame");
		capture.Configure("capture-a", 0, 2);
		suite.Expect(!capture.ConsumeCaptureStart(),
			"unchanged quality capture configuration cannot retrigger its reset");
		capture.Configure("capture-b", 3, 2);
		suite.Expect(!capture.CapturesCurrentFrame() && !capture.ConsumeCaptureStart(),
			"quality capture does not reset during skipped warm-up frames");
		capture.Configure("capture-c", 0, 2);
		std::string snapshotError;
		QualityCaptureTextures missingTextures;
		QualityCaptureMetadata missingMetadata;missingMetadata.frameId=17;
		suite.Expect(!capture.Capture(nullptr,nullptr,missingMetadata,missingTextures,snapshotError)
			&&capture.CapturedPvrSnapshot(17)==nullptr&&capture.CapturedPvrSnapshot(16)==nullptr,
			"quality failed capture exposes no requested or stale snapshot");
		capture.Configure("capture-d", 0, 2);
		suite.Expect(capture.ConsumeCaptureStart(),
			"new quality capture configuration rearms its temporal reset");
		capture.Configure("capture-absolute", 9999, 2, false, 1782);
		capture.SetSourceFrame(1781);
		suite.Expect(!capture.CapturesCurrentFrame()&&!capture.ConsumeCaptureStart(),"absolute capture waits for renderer frame");
		capture.SetSourceFrame(1782);
		suite.Expect(capture.CapturesCurrentFrame()&&capture.ConsumeCaptureStart(),"absolute capture ignores bypass-dependent skip count");
		capture.Configure("capture-absolute", 9999, 2, false, 1782);
		suite.Expect(!capture.ConsumeCaptureStart(),"same absolute capture cannot rearm reset");
		capture.Configure("capture-absolute", 9999, 2, false, 1783);
		capture.SetSourceFrame(1783);
		suite.Expect(capture.ConsumeCaptureStart(),"changed absolute target rearms capture reset");
		capture.Configure("capture-producer",0,2,false,0,1829);
		capture.SetSourceFrame(9999,1828);
		suite.Expect(!capture.CapturesCurrentFrame(),"producer capture does not use renderer counter as source identity");
		capture.SetSourceFrame(1782,1829);
		suite.Expect(capture.CapturesCurrentFrame()&&capture.ConsumeCaptureStart(),"producer capture starts at requested game ordinal");
	}
	{
		const auto defaultOrigin = GetEvidenceMarkerOrigin(640, 480, false);
		const auto oitOrigin = GetEvidenceMarkerOrigin(640, 480, true);
		const auto clampedOrigin = GetEvidenceMarkerOrigin(16, 24, true);
		suite.Expect(defaultOrigin.x == 0 && defaultOrigin.y == 0,
			"evidence marker defaults to the Gate 10 top-left origin");
		suite.Expect(oitOrigin.x == 608 && oitOrigin.y == 448,
			"evidence marker bottom-right origin is output-relative");
		suite.Expect(clampedOrigin.x == 0 && clampedOrigin.y == 0,
			"evidence marker origin clamps for undersized outputs");
	}
	{
		const auto faithful = ResolveQualityProfile(0, 0);
		const auto enhanced = ResolveQualityProfile(1, 3);
		const auto sprite = ResolveQualityProfile(2, 6);
		const auto uncanny = ResolveQualityProfile(3, 1);
		suite.Expect(faithful.faithful && faithful.conservativeTemporalMask
			&& faithful.protectCharacters && !faithful.bypassGenerative,
			"Faithful Dreamcast Remaster is the conservative default profile");
		suite.Expect(enhanced.faithful && !enhanced.conservativeTemporalMask
			&& enhanced.protectCharacters && std::string(enhanced.styleName) == "Cel-shaded",
			"Enhanced Materials retains character protection and style metadata");
		suite.Expect(!sprite.faithful && sprite.bypassGenerative
			&& sprite.externalRecommendation.find("user controlled") != std::string::npos,
			"sprite-heavy Photoreal profile remains explicit and recommends bypass");
		suite.Expect(!uncanny.faithful && !uncanny.conservativeTemporalMask
			&& !uncanny.protectCharacters && !uncanny.bypassGenerative
			&& std::string(uncanny.name) == "Uncanny Cinematic"
			&& uncanny.externalRecommendation.find("Structure Intensity 200%") != std::string::npos,
			"Uncanny Cinematic is a selectable non-faithful maximum-coverage profile");
	}
	{
		LiveStatus published;
		published.rendererAvailable = true;
		published.active = true;
		published.mode = NeuralMode::Dlss5Experimental;
		published.api = Api::D3D12;
		published.lastSubmit = SubmitStatus::Busy;
		published.reason = "ring busy";
		published.stage.submissions = 17;
		published.stage.busySkips = 2;
		published.stage.fallbacks = 1;
		published.qualityProfile = 3;
		published.dlssPreset = 11;
		published.rasterJitterX = -.375f;
		published.rasterJitterY = -.0555556f;
		published.rasterJitterApplied = true;
		published.renderWidth = 640;
		published.renderHeight = 480;
		published.outputWidth = 640;
		published.outputHeight = 480;
		published.sourceFrameId = 42;
		PublishLiveStatus(published);
		const auto copied = GetLiveStatus();
		suite.Expect(copied.rendererAvailable && copied.active
			&& copied.mode == NeuralMode::Dlss5Experimental && copied.api == Api::D3D12
			&& copied.lastSubmit == SubmitStatus::Busy && copied.reason == "ring busy"
			&& copied.stage.submissions == 17 && copied.sourceFrameId == 42
			&& copied.generation != 0,
			"live neural status publishes a self-contained UI snapshot");
		suite.Expect(std::string(NeuralModeName(copied.mode)) == "DLSS 5 Experimental"
			&& std::string(SubmitStatusName(copied.lastSubmit)) == "Busy"
			&& std::string(ApiName(copied.api)) == "D3D11On12 / D3D12"
			&& std::string(DlssPresetName(copied.dlssPreset)) == "K",
			"live neural status exposes stable developer labels");
		const auto overlay = FormatLiveStatusOverlay(copied, 60.f);
		suite.Expect(overlay.find("DLSS 5 Experimental | Uncanny | Preset K | D3D11On12")
			!= std::string::npos
			&& overlay.find("Public contract: Busy | External unverified | 640x480 -> 640x480")
			!= std::string::npos
			&& overlay.find("J -0.375,-0.056") != std::string::npos
			&& overlay.find("FPS 60.0 | Frame 16.7 ms") != std::string::npos
			&& overlay.find("Accepted 17 | Busy 2 | Fallback 1 | Drops n/a")
			!= std::string::npos,
			"live neural status formats the compact late-OSD contract");
		const auto publishedGeneration = copied.generation;
		ResetLiveStatus();
		const auto reset = GetLiveStatus();
		suite.Expect(!reset.rendererAvailable && !reset.active
			&& reset.mode == NeuralMode::Off && reset.generation > publishedGeneration,
			"live neural status reset removes stale renderer state");
	}
	{
		std::string error;
		const bool valid = ValidateProductionExportShader(error);
		suite.Expect(valid, "production native and neural-export vertex/pixel shaders compile");
		if (!valid && !error.empty())
			std::cerr << error << '\n';
	}
	const DrawRecord base = BaseDraw();
	suite.Expect(DrawSignature(base) == DrawSignature(base), "draw signature deterministic");
	auto changed = base;
	changed.texId++;
	suite.Expect(DrawSignature(base) != DrawSignature(changed), "draw signature changes with identity");
	changed = base;
	changed.zMax += .01f;
	suite.Expect(DrawSignature(base) != DrawSignature(changed), "draw signature covers depth bounds");
	changed = base;
	changed.centroid[0] += 24.f;
	changed.bboxMin[0] += 24;
	changed.bboxMax[0] += 24;
	suite.Expect(DrawStructuralSignature(base) == DrawStructuralSignature(changed)
		&& DrawSignature(base) != DrawSignature(changed),
		"structural identity is independent of pose");
	changed = base;
	changed.textureGeneration++;
	suite.Expect(DrawStructuralSignature(base) == DrawStructuralSignature(changed),
		"texture content generation is separate from structural identity");

	{
		DrawRecord previous[] = {base};
		DrawRecord current[] = {base};
		const auto result = MatchDraws({previous, 1}, {current, 1});
		suite.Expect(result.size() == 1 && result[0].tier == 1 && Near(result[0].confidence, 1.f),
			"matcher tier 1 exact");
	}
	{
		DrawRecord previous[] = {base};
		DrawRecord current[] = {base};
		current[0].topologySig++;
		current[0].ordinal = 30;
		const auto result = MatchDraws({previous, 1}, {current, 1});
		suite.Expect(result[0].tier == 2 && result[0].confidence >= .5f
			&& result[0].confidence < .8f && result[0].bestCost > 0.f,
			"matcher tier 2 reordered structural");
	}
	{
		DrawRecord previous[] = {BaseDraw(3), BaseDraw(8)};
		previous[1].topologySig = 0x999;
		DrawRecord current[] = {previous[1], previous[0]};
		const auto result = MatchDraws({previous, 2}, {current, 2});
		suite.Expect(result[0].prevOrdinal == 8 && result[1].prevOrdinal == 3,
			"matcher one-to-one duplicate textures");
	}
	{
		DrawRecord previous[] = {BaseDraw(0), BaseDraw(1)};
		previous[0].centroid[0] = 10.f;
		previous[1].centroid[0] = 110.f;
		DrawRecord current[] = {BaseDraw(0), BaseDraw(1)};
		current[0].centroid[0] = 108.f;
		current[1].centroid[0] = 12.f;
		const auto result = MatchDraws({previous, 2}, {current, 2});
		suite.Expect(result[0].prevOrdinal == 1 && result[1].prevOrdinal == 0
			&& result[0].bestCost < result[0].secondBestCost
			&& result[1].bestCost < result[1].secondBestCost,
			"minimum-cost assignment follows repeated-object pose across reorder");
	}
	{
		DrawRecord previous[] = {BaseDraw(11), BaseDraw(12)};
		previous[0].topologySig = 101;
		previous[0].centroid[0] = 10.f;
		previous[1].topologySig = 102;
		previous[1].centroid[0] = 110.f;
		DrawRecord current[] = {BaseDraw(0), BaseDraw(1)};
		current[0].topologySig = 201;
		current[0].centroid[0] = 108.f;
		current[1].topologySig = 202;
		current[1].centroid[0] = 12.f;
		const auto result = MatchDraws({previous, 2}, {current, 2});
		suite.Expect(result[0].tier == 2 && result[1].tier == 2
			&& result[0].prevOrdinal == 12 && result[1].prevOrdinal == 11
			&& result[0].bestCost < result[0].secondBestCost
			&& result[1].bestCost < result[1].secondBestCost,
			"structural bucket uses minimum-cost one-to-one assignment");
	}
	{
		DrawRecord previous[9];
		DrawRecord current[9];
		for (std::uint16_t i = 0; i < 9; ++i)
		{
			previous[i] = BaseDraw(i);
			current[i] = BaseDraw(i);
		}
		const auto result = MatchDraws({previous, 9}, {current, 9});
		const bool allAmbiguous = std::all_of(result.begin(), result.end(), [](const DrawMatch& match) {
			return match.confidence == 0.f &&
				match.reason == static_cast<std::uint8_t>(MatchReason::Ambiguous);
		});
		suite.Expect(allAmbiguous, "large repeated bucket is reactive-ambiguous with zero motion trust");
	}
	{
		for (int generationKind = 0; generationKind < 3; ++generationKind)
		{
			DrawRecord previous[] = {base};
			DrawRecord current[] = {base};
			if (generationKind == 0) ++current[0].textureGeneration;
			if (generationKind == 1) ++current[0].paletteGeneration;
			if (generationKind == 2) ++current[0].rttGeneration;
			const auto result = MatchDraws({previous, 1}, {current, 1});
			suite.Expect(result[0].tier == 0 && result[0].confidence == 0.f,
				generationKind == 0 ? "same-address texture replacement is untrusted" :
				generationKind == 1 ? "palette replacement is untrusted" :
				"rendered-texture replacement is untrusted");
		}
	}
	{
		DrawRecord previous[] = {base};
		DrawRecord current[] = {base};
		current[0].uvSig++;
		current[0].ordinal = 20;
		const auto result = MatchDraws({previous, 1}, {current, 1});
		suite.Expect(result[0].tier == 3 && Near(result[0].confidence, .5f),
			"matcher tier 3 similarity");
	}
	{
		DrawRecord previous = base;
		DrawRecord current = base;
		current.vertexCount += 2;
		current.indexCount += 2;
		current.topologySig++;
		current.ordinal += 8;
		current.bboxMin[0] += 4;
		const auto result = MatchDraws({&previous, 1}, {&current, 1});
		suite.Expect(result[0].tier == 2 && StripCoverage(previous, current) >= .9f,
			"strip-level changed-count match retains covered region");
	}
	{
		DrawRecord reactive = base;
		reactive.list = 2;
		reactive.bboxMax[0] = reactive.bboxMin[0] + 8;
		reactive.bboxMax[1] = reactive.bboxMin[1] + 8;
		suite.Expect(IsReactive(reactive), "small translucent draw is reactive");
		DrawRecord previous[] = {reactive};
		DrawRecord current[] = {reactive};
		const auto result = MatchDraws({previous, 1}, {current, 1});
		suite.Expect(result[0].confidence == 0.f &&
			result[0].reason == static_cast<std::uint8_t>(MatchReason::Reactive),
			"reactive draw emits no trusted match");
	}
	{
		DrawRecord hud = base;
		hud.list = 2;
		hud.flags = DrawScreenAligned;
		hud.zMin = hud.zMax = .5f;
		hud.bboxMin[0] = 2; hud.bboxMin[1] = 8;
		hud.bboxMax[0] = 42; hud.bboxMax[1] = 28;
		hud.ordinal = 18;
		suite.Expect(IsHighConfidenceOverlay(hud, 20, 320, 240, 3, 4),
			"stable late edge-aligned repeated-texture draw is a high-confidence overlay");
		DrawRecord interior = hud;
		interior.bboxMin[0] = 100; interior.bboxMax[0] = 140;
		interior.bboxMin[1] = 80; interior.bboxMax[1] = 100;
		DrawRecord physical = hud;
		physical.list = 0;
		DrawRecord perspective = hud;
		perspective.zMax += .02f;
		DrawRecord earlySingle = hud;
		earlySingle.ordinal = 0;
		DrawRecord earlyBatch = earlySingle;
		earlyBatch.screenAlignedPrimitiveCount = 2;
		suite.Expect(!IsHighConfidenceOverlay(interior, 20, 320, 240, 3, 4)
			&& !IsHighConfidenceOverlay(physical, 20, 320, 240, 3, 4)
			&& !IsHighConfidenceOverlay(perspective, 20, 320, 240, 3, 4)
			&& !IsHighConfidenceOverlay(hud, 20, 320, 240, 2, 4)
			&& !IsHighConfidenceOverlay(hud, 20, 320, 240, 3, 1)
			&& !IsHighConfidenceOverlay(earlySingle, 20, 320, 240, 3, 4)
			&& IsHighConfidenceOverlay(earlyBatch, 20, 320, 240, 3, 4),
			"world negatives and early single quads fail while a stable proven edge batch need not be late");
		DrawRecord soulcaliburBar = earlySingle;
		soulcaliburBar.list = 2;
		soulcaliburBar.ordinal = 18;
		soulcaliburBar.bboxMin[0] = 20; soulcaliburBar.bboxMin[1] = 37;
		soulcaliburBar.bboxMax[0] = 272; soulcaliburBar.bboxMax[1] = 65;
		soulcaliburBar.zMin = soulcaliburBar.zMax = .198f;
		DrawRecord soulcaliburCounter = soulcaliburBar;
		soulcaliburCounter.bboxMin[0] = 236; soulcaliburCounter.bboxMin[1] = 66;
		soulcaliburCounter.bboxMax[0] = 276; soulcaliburCounter.bboxMax[1] = 86;
		DrawRecord layeredName = soulcaliburCounter;
		layeredName.screenAlignedPrimitiveCount = 2;
		layeredName.bboxMin[0] = 26; layeredName.bboxMax[0] = 97;
		layeredName.zMin = .178f; layeredName.zMax = .198f;
		DrawRecord worldControl = soulcaliburBar;
		worldControl.bboxMin[1] = 140; worldControl.bboxMax[1] = 168;
		suite.Expect(IsTitleSpecificOverlay(soulcaliburBar, 20, 640, 480, 3,
				OverlayProfile::SoulcaliburT1401nHudV1)
			&& IsTitleSpecificOverlay(soulcaliburCounter, 20, 640, 480, 3,
				OverlayProfile::SoulcaliburT1401nHudV1)
			&& IsTitleSpecificOverlay(layeredName, 20, 640, 480, 3,
				OverlayProfile::SoulcaliburT1401nHudV1)
			&& !IsTitleSpecificOverlay(worldControl, 20, 640, 480, 3,
				OverlayProfile::SoulcaliburT1401nHudV1)
			&& !IsTitleSpecificOverlay(soulcaliburBar, 20, 640, 480, 3,
				OverlayProfile::None),
			"Soulcalibur profile admits captured top HUD controls without weakening the generic world negative");
		DrawRecord header=soulcaliburBar;header.list=4;header.texId=671530672u;header.blend=37;
		header.flags=DrawScreenAligned;header.screenAlignedPrimitiveCount=23;
		header.bboxMin[0]=28;header.bboxMin[1]=22;header.bboxMax[0]=577;header.bboxMax[1]=38;
		const auto capturedHud=[](const DrawRecord& draw){return IsTitleSpecificOverlay(draw,20,640,480,0,OverlayProfile::SoulcaliburT1401nHudV1);};
		suite.Expect(capturedHud(header),"captured Soulcalibur punch-through header survives animated history");
		auto timer=header;timer.texId=696696496u;timer.bboxMin[0]=282;timer.bboxMin[1]=26;timer.bboxMax[0]=358;timer.bboxMax[1]=76;
		suite.Expect(capturedHud(timer),"captured Soulcalibur timer atlas is protected");
		auto fill=soulcaliburBar;fill.blend=37;fill.texId=795315888u;fill.bboxMin[1]=38;fill.bboxMax[1]=64;
		suite.Expect(capturedHud(fill),"captured Soulcalibur health fill survives palette churn");
		auto name=header;name.list=2;name.texId=686272176u;name.bboxMin[0]=26;name.bboxMin[1]=62;name.bboxMax[0]=614;name.bboxMax[1]=86;
		name.zMin=.162346f;name.zMax=.180385f;
		suite.Expect(capturedHud(name),"captured Soulcalibur layered names retain both sides");
		auto counter=soulcaliburCounter;counter.blend=37;counter.texId=739217920u;
		suite.Expect(capturedHud(counter),"captured Soulcalibur counters are protected");
		auto wrong=header;wrong.texId++;
		suite.Expect(!capturedHud(wrong),"captured HUD rejects unknown texture");
		wrong=header;wrong.bboxMax[1]=140;
		suite.Expect(!capturedHud(wrong),"captured HUD rejects world region");
		wrong=header;wrong.zMin=.01f;
		suite.Expect(!capturedHud(wrong),"captured HUD rejects world depth");
		wrong=header;wrong.flags|=DrawRtt;
		suite.Expect(!capturedHud(wrong),"captured HUD rejects RTT");
		suite.Expect(!IsTitleSpecificOverlay(header,20,640,480,0,OverlayProfile::None),"captured HUD is title scoped");
	}
	{
		DrawRecord menu[4]{};
		for (std::size_t i = 0; i < 4; ++i)
		{
			menu[i].flags = DrawScreenAligned;
			menu[i].zMin = menu[i].zMax = .5f;
			menu[i].bboxMin[0] = static_cast<std::int16_t>((i % 2) * 160);
			menu[i].bboxMin[1] = static_cast<std::int16_t>((i / 2) * 120);
			menu[i].bboxMax[0] = menu[i].bboxMin[0] + 160;
			menu[i].bboxMax[1] = menu[i].bboxMin[1] + 120;
		}
		suite.Expect(IsPredominantly2DFrame({menu, 4}, 320, 240),
			"tiled screen-aligned menu is conservatively classified as predominantly 2D");
		DrawRecord mixed[5] = {menu[0], menu[1], menu[2], menu[3], menu[0]};
		mixed[4].flags = 0;
		mixed[4].zMin = .1f; mixed[4].zMax = .9f;
		mixed[4].bboxMin[0] = 20; mixed[4].bboxMin[1] = 20;
		mixed[4].bboxMax[0] = 300; mixed[4].bboxMax[1] = 220;
		suite.Expect(!IsPredominantly2DFrame({mixed, 5}, 320, 240),
			"mixed 3D scene with screen-aligned HUD does not trigger the 2D bypass");
		std::uint8_t enter = 0;
		std::uint8_t exit = 0;
		bool active = false;
		active = UpdateConservativeBypass(true, active, enter, exit);
		active = UpdateConservativeBypass(false, active, enter, exit);
		active = UpdateConservativeBypass(true, active, enter, exit);
		suite.Expect(!active, "transient 2D classification cannot enter the conservative bypass");
		active = UpdateConservativeBypass(true, active, enter, exit);
		active = UpdateConservativeBypass(true, active, enter, exit);
		suite.Expect(active, "three consecutive 2D frames enter the conservative bypass");
		active = UpdateConservativeBypass(false, active, enter, exit);
		active = UpdateConservativeBypass(true, active, enter, exit);
		active = UpdateConservativeBypass(false, active, enter, exit);
		suite.Expect(active, "transient 3D classification cannot leave the conservative bypass");
		active = UpdateConservativeBypass(false, active, enter, exit);
		active = UpdateConservativeBypass(false, active, enter, exit);
		suite.Expect(!active, "three consecutive 3D frames leave the conservative bypass");
	}
	{
		rend_context context{};
		context.framebufferWidth = 320;
		context.framebufferHeight = 240;
		context.verts.resize(6);
		for (std::size_t i = 0; i < context.verts.size(); ++i)
		{
			context.verts[i].x = static_cast<float>(10 + i * 11);
			context.verts[i].y = static_cast<float>(20 + i * 7);
			context.verts[i].z = .2f + static_cast<float>(i) * .01f;
		}
		context.idx = {0, 1, 2, ~u32{0}, 3, 4, 5};
		PolyParam poly{};
		poly.init();
		poly.first = 0;
		poly.count = static_cast<u32>(context.idx.size());
		poly.tcw.full = 77;
		context.global_param_op.push_back(poly);
		RenderPass pass{};
		pass.op_count = 1;
		context.render_passes.push_back(pass);
		auto instrumentation = std::make_unique<NeuralInstrumentation>();
		instrumentation->SetEnabled(true);
		suite.Expect(instrumentation->AcceptedEvaluationCount() == 0,
			"jitter phase begins at zero after enable discontinuity");
		const auto& first = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		instrumentation->MarkEvaluated(first.frameId);
		suite.Expect(instrumentation->AcceptedEvaluationCount() == 1,
			"jitter phase advances only after accepted evaluation");
		for (auto& vertex : context.verts) vertex.x += 4.f;
		const auto& moved = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		suite.Expect(moved.matches.data[0].confidence >= .5f && moved.historyValid
			&& !moved.sceneCut && instrumentation->TrustedPreviousVertexCount() == 6,
			"primitive-restart strip breaks preserve trusted previous positions");
		instrumentation->Discontinuity();
		suite.Expect(instrumentation->AcceptedEvaluationCount() == 0,
			"jitter phase resets with accepted-history discontinuity");
	}
	{
		rend_context context{};
		context.framebufferWidth = 320;
		context.framebufferHeight = 240;
		context.verts.resize(12);
		auto setQuad = [&](int baseVertex, float left, float top, float right, float bottom,
			float uvOffset) {
			const float positions[4][2] = {{left, top}, {left, bottom},
				{right, top}, {right, bottom}};
			for (int i = 0; i < 4; ++i)
			{
				auto& vertex = context.verts[baseVertex + i];
				vertex.x = positions[i][0]; vertex.y = positions[i][1]; vertex.z = .5f;
				vertex.u = uvOffset + (i >= 2 ? .1f : 0.f);
				vertex.v = i % 2 ? .1f : 0.f;
			}
		};
		setQuad(0, 2, 8, 34, 24, 0.f);
		setQuad(4, 2, 32, 48, 48, .2f);
		setQuad(8, 270, 70, 310, 100, .4f);
		context.idx = {0,1,2,3, 4,5,6,7, 8,9,10,11};
		for (int i = 0; i < 3; ++i)
		{
			PolyParam poly{};
			poly.init(); poly.first = i * 4; poly.count = 4; poly.tcw.full = 71;
			context.global_param_tr.push_back(poly);
		}
		RenderPass pass{};
		pass.tr_count = 3;
		context.render_passes.push_back(pass);
		auto instrumentation = std::make_unique<NeuralInstrumentation>();
		instrumentation->SetEnabled(true);
		for (int frameIndex = 0; frameIndex < 3; ++frameIndex)
		{
			if (frameIndex != 0)
			{
				for (int i = 8; i < 12; ++i) context.verts[i].y += 1.f;
			}
			const auto& frame = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
				320, 240, {0,0,320,240}, {});
			instrumentation->MarkEvaluated(frame.frameId);
		}
		suite.Expect(instrumentation->OverlayDrawCount() == 2
			&& instrumentation->IsOverlayOrdinal(0)
			&& instrumentation->IsOverlayOrdinal(1)
			&& !instrumentation->IsOverlayOrdinal(2),
			"accepted-frame overlay classifier protects stable HUD quads and rejects moving world control");
	}
	{
		// A real renderer may merge many HUD quads into one indexed strip batch.
		// UV/topology churn is permitted, but every strip must independently prove
		// an axis-aligned rectangle and the draw must remain stationary.
		rend_context context{};
		context.globClip = {640, 480};
		context.framebufferWidth = 1920;
		context.framebufferHeight = 1440;
		context.verts.resize(24);
		auto quad = [&](int base, float left, float top, float right, float bottom) {
			const float xy[4][2] = {{left,top},{left,bottom},{right,top},{right,bottom}};
			for (int i = 0; i < 4; ++i)
			{
				auto& v = context.verts[base + i];
				v.x = xy[i][0]; v.y = xy[i][1]; v.z = .5f;
				v.u = i >= 2 ? 1.f : 0.f; v.v = i & 1 ? 1.f : 0.f;
			}
		};
		quad(0, 20, 37, 90, 65);
		quad(4, 91, 37, 180, 65);
		quad(8, 181, 37, 272, 65);
		quad(12, 4, 110, 48, 150);
		context.verts[15].x += 3.f; // deliberate skewed negative
		quad(16, 120, 160, 160, 180);
		quad(20, 161, 160, 201, 180);
		context.idx = {0,1,2,3,~u32{0},4,5,6,7,~u32{0},8,9,10,11,
			12,13,14,15, 16,17,18,19,~u32{0},20,21,22,23};
		for (const auto range : {std::pair<u32,u32>{0,14}, {14,4}, {18,9}})
		{
			PolyParam poly{}; poly.init(); poly.first = range.first; poly.count = range.second;
			poly.tcw.full = 71; context.global_param_tr.push_back(poly);
		}
		RenderPass pass{}; pass.tr_count = 3; context.render_passes.push_back(pass);
		auto instrumentation = std::make_unique<NeuralInstrumentation>();
		instrumentation->SetEnabled(true);
		for (int frameIndex = 0; frameIndex < 3; ++frameIndex)
		{
			for (int i = 0; i < 12; ++i) context.verts[i].u += .03125f;
			if (frameIndex == 1)
			{
				std::swap(context.idx[0], context.idx[2]);
				std::swap(context.idx[5], context.idx[7]);
			}
			const auto& frame = instrumentation->CaptureGeometry(context, {}, {}, 1920, 1440,
				1920, 1440, {0,0,1920,1440}, {});
			instrumentation->MarkEvaluated(frame.frameId);
		}
		const auto diagnostics = instrumentation->CaptureOverlayDiagnostics();
		suite.Expect(instrumentation->OverlayDrawCount() == 1
			&& instrumentation->IsOverlayOrdinal(0)
			&& !instrumentation->IsOverlayOrdinal(1)
			&& !instrumentation->IsOverlayOrdinal(2),
			"merged HUD strips survive UV/topology churn while skewed and interior controls fail");
		suite.Expect(diagnostics.size() == 3
			&& diagnostics[0].draw.screenAlignedPrimitiveCount == 3
			&& diagnostics[0].textureUseCount == 3
			&& diagnostics[1].draw.screenAlignedPrimitiveCount == 0
			&& diagnostics[2].draw.screenAlignedPrimitiveCount == 2,
			"quad-batch evidence counts proven rectangles without degenerate texture inflation");
		const auto& frame = instrumentation->CaptureGeometry(context, {}, {}, 1920, 1440,
			1920, 1440, {0,0,1920,1440}, {});
		suite.Expect(frame.screenWidth == 640 && frame.screenHeight == 480,
			"overlay anchoring uses native PVR coordinates under target-matched raster scaling");
		instrumentation->SetOverlayGameId("T1401N");
		for (int i = 12; i < 16; ++i) context.verts[i].y -= 80.f;
		for (int frameIndex = 0; frameIndex < 3; ++frameIndex)
		{
			for (int i = 12; i < 16; ++i) context.verts[i].u += .0625f;
			std::swap(context.idx[14], context.idx[16]);
			const auto& titleFrame = instrumentation->CaptureGeometry(context, {}, {}, 1920, 1440,
				1920, 1440, {0,0,1920,1440}, {});
			instrumentation->MarkEvaluated(titleFrame.frameId);
		}
		suite.Expect(instrumentation->IsOverlayOrdinal(1),
			"Soulcalibur profile continuity retains a captured top-HUD batch across UV/topology churn");
	}
	{
		// Normal sorted translucency submits each Soulcalibur character name as
		// two coincident quads at distinct depths. The exact title profile pairs
		// this bounded duplicate bucket one-to-one; a changed bucket is rejected.
		rend_context context{};
		context.globClip = {640, 480};
		context.framebufferWidth = 640; context.framebufferHeight = 480;
		context.verts.resize(12);
		context.idx = {0,1,2,3, 4,5,6,7, 8,9,10,11};
		for (int layer = 0; layer < 3; ++layer)
		{
			const float xy[4][2] = {{26,62},{26,86},{97,62},{97,86}};
			for (int i = 0; i < 4; ++i)
			{
				auto& vertex = context.verts[layer * 4 + i];
				vertex.x = xy[i][0]; vertex.y = xy[i][1];
				vertex.z = .18f + layer * .02f;
				vertex.u = i >= 2 ? 1.f : 0.f; vertex.v = i & 1 ? 1.f : 0.f;
			}
			PolyParam poly{}; poly.init(); poly.first = layer * 4; poly.count = 4;
			poly.tcw.full = 93; context.global_param_tr.push_back(poly);
		}
		context.global_param_tr.resize(2);
		RenderPass pass{}; pass.tr_count = 2; context.render_passes.push_back(pass);
		auto instrumentation = std::make_unique<NeuralInstrumentation>();
		instrumentation->SetEnabled(true);
		instrumentation->SetOverlayGameId("T1401N");
		std::vector<OverlayDrawDiagnostic> accepted;
		for (int frameIndex = 0; frameIndex < 3; ++frameIndex)
		{
			for (int i = 0; i < 8; ++i) context.verts[i].u += .03125f;
			const auto& frame = instrumentation->CaptureGeometry(context, {}, {}, 640, 480,
				640, 480, {0,0,640,480}, {});
			if (frameIndex == 2) accepted = instrumentation->CaptureOverlayDiagnostics();
			instrumentation->MarkEvaluated(frame.frameId);
		}
		const bool duplicateContinuity = instrumentation->OverlayDrawCount() == 2
			&& instrumentation->IsOverlayOrdinal(0)
			&& instrumentation->IsOverlayOrdinal(1)
			&& accepted.size() == 2
			&& accepted[0].stableAcceptedFrames == 3
			&& accepted[1].stableAcceptedFrames == 3;
		suite.Expect(duplicateContinuity,
			"Soulcalibur duplicate name layers retain one-to-one depth continuity"
			+ std::string(" (overlays=") + std::to_string(instrumentation->OverlayDrawCount())
			+ ", stability=" + (accepted.empty() ? std::string("none")
				: std::to_string(accepted[0].stableAcceptedFrames)) + ")");
		context.global_param_tr.push_back([&] {
			PolyParam poly{}; poly.init(); poly.first = 8; poly.count = 4;
			poly.tcw.full = 93; return poly;
		}());
		context.render_passes[0].tr_count = 3;
		const auto& changedFrame = instrumentation->CaptureGeometry(context, {}, {}, 640, 480,
			640, 480, {0,0,640,480}, {});
		instrumentation->MarkEvaluated(changedFrame.frameId);
		suite.Expect(instrumentation->OverlayDrawCount() == 0,
			"Soulcalibur duplicate overlay continuity rejects a changed occurrence count");
	}
	{
		// Sorted translucency arrives as triangle lists; accept only complete
		// two-triangle rectangles, never a repeated single-triangle control.
		rend_context context{};
		context.globClip = {640, 480};
		context.framebufferWidth = 640; context.framebufferHeight = 480;
		context.verts.resize(12);
		const float xy[12][2] = {{28,22},{28,38},{120,22},{120,38},
			{121,22},{121,38},{220,22},{220,38}, {4,4},{4,24},{44,4},{44,24}};
		for (int i = 0; i < 12; ++i)
		{
			context.verts[i].x=xy[i][0]; context.verts[i].y=xy[i][1];
			context.verts[i].z=.5f; context.verts[i].u=float(i)/12.f;
		}
		context.idx = {0,1,2,2,1,3, 4,5,6,6,5,7, 8,9,10,8,9,10};
		for (int i = 0; i < 2; ++i)
		{
			PolyParam poly{}; poly.init(); poly.first = i * 4; poly.count = 4;
			poly.tcw.full = 85; context.global_param_tr.push_back(poly);
		}
		context.sortedTriangles = {{0,0,12},{1,12,6}};
		RenderPass pass{}; pass.tr_count=2; pass.autosort=true; pass.sorted_tr_count=2;
		context.render_passes.push_back(pass);
		auto instrumentation = std::make_unique<NeuralInstrumentation>();
		instrumentation->SetEnabled(true);
		for (int frameIndex = 0; frameIndex < 3; ++frameIndex)
		{
			for (auto& vertex : context.verts) vertex.u += .015625f;
			const auto& frame = instrumentation->CaptureGeometry(context, {}, {}, 640, 480,
				640, 480, {0,0,640,480}, {});
			instrumentation->MarkEvaluated(frame.frameId);
		}
		const auto diagnostics = instrumentation->CaptureOverlayDiagnostics();
		suite.Expect(diagnostics.size() == 4
			&& diagnostics[2].draw.screenAlignedPrimitiveCount == 2
			&& diagnostics[3].draw.screenAlignedPrimitiveCount == 0
			&& instrumentation->IsOverlayOrdinal(2)
			&& !instrumentation->IsOverlayOrdinal(3),
			"sorted quad batches classify while an incomplete triangle-pair control fails");
	}
	{
		// Sorted translucent PolyParam.first still addresses vertices. Only the
		// SortedTriangle ranges address the submitted index buffer. Deliberately
		// put unrelated world indices at the same numeric offset as the HUD strip.
		rend_context context;
		context.framebufferWidth = 320;
		context.framebufferHeight = 240;
		context.verts.resize(8);
		const float positions[8][2] = {{2,8},{2,24},{34,8},{34,24},
			{120,100},{120,140},{160,100},{160,140}};
		for (int i = 0; i < 8; ++i)
		{
			context.verts[i].x = positions[i][0];
			context.verts[i].y = positions[i][1];
			context.verts[i].z = i < 4 ? .8f : .2f;
		}
		context.idx = {4,5,6,7, 0,1,2, 2,1,3};
		PolyParam strip{};
		strip.init(); strip.first = 0; strip.count = 4; strip.tcw.full = 71;
		context.global_param_tr.push_back(strip);
		RenderPass pass{};
		pass.tr_count = 1; pass.autosort = true; pass.sorted_tr_count = 1;
		context.render_passes.push_back(pass);
		context.sortedTriangles.push_back({0, 4, 6});
		auto instrumentation = std::make_unique<NeuralInstrumentation>();
		instrumentation->SetEnabled(true);
		const auto& sorted = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0,0,320,240}, {});
		const bool correctSorted = sorted.draws.size == 2
			&& sorted.draws.data[0].indexCount == 0
			&& (sorted.draws.data[0].flags & DrawDegenerate) != 0
			&& sorted.draws.data[1].firstIndex == 4 && sorted.draws.data[1].indexCount == 6
			&& sorted.draws.data[1].bboxMin[0] == 2 && sorted.draws.data[1].bboxMax[0] == 34
			&& sorted.draws.data[1].zMin == .8f && sorted.draws.data[1].zMax == .8f;
		suite.Expect(correctSorted,
			"sorted translucent capture follows submitted triangles, not vertex-offset alias control");
		const auto diagnostics = instrumentation->CaptureOverlayDiagnostics();
		suite.Expect(diagnostics.size() == 2 && diagnostics[1].draw.ordinal == 1
			&& !diagnostics[1].classified && diagnostics[1].stableAcceptedFrames == 1,
			"bounded draw diagnostics describe the actual sorted submission ordinal");
		instrumentation->MarkEvaluated(sorted.frameId);
		const auto& repeated = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0,0,320,240}, {});
		suite.Expect(repeated.draws.size == 2 && repeated.matches.data[1].confidence == 0.f
			&& (repeated.draws.data[1].flags & (DrawTriangleList | DrawReactive))
				== (DrawTriangleList | DrawReactive)
			&& instrumentation->TrustedPreviousVertexCount() == 0,
			"sorted translucent submissions cannot manufacture authoritative geometry motion");
		// A later indexed pass must keep its own original-list ordinal. Multiple
		// sorted spans sharing one state owner must remain distinct submissions.
		context.global_param_tr.push_back(strip);
		RenderPass later = pass;
		later.tr_count = 2;
		context.render_passes.push_back(later);
		context.sortedTriangles[0].count = 3;
		context.sortedTriangles.push_back({0, 7, 3});
		context.render_passes[0].sorted_tr_count = 2;
		context.render_passes[1].sorted_tr_count = 2;
		const auto& mixed = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0,0,320,240}, {});
		suite.Expect(mixed.draws.size == 4 && mixed.draws.data[0].indexCount == 0
			&& mixed.draws.data[1].indexCount == 4 && mixed.draws.data[1].pass == 1
			&& mixed.draws.data[2].firstIndex == 4 && mixed.draws.data[2].pass == 0
			&& mixed.draws.data[3].firstIndex == 7 && mixed.draws.data[3].pass == 0,
			"mixed passes preserve indexed ordinals and separate sorted spans sharing render state");
		context.global_param_tr.pop_back();
		context.render_passes.pop_back();
		context.sortedTriangles.clear();
		context.render_passes[0].sorted_tr_count = 0;
		const auto& indexed = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0,0,320,240}, {});
		suite.Expect(indexed.draws.size == 1 && indexed.draws.data[0].firstIndex == 0
			&& indexed.draws.data[0].bboxMin[0] == 120 && indexed.draws.data[0].bboxMax[0] == 160,
			"indexed OIT or per-strip translucent capture retains its original index contract");
	}
	{
		DrawRecord current = base;
		current.list = 3;
		DrawRecord previous[] = {base};
		const auto result = MatchDraws({previous, 1}, {&current, 1});
		suite.Expect(result[0].confidence == 0.f && result[0].tier == 0,
			"unmatched draw has zero confidence");
	}
	{
		Point2 current[] = {{-1.f, 0.f}, {1.f, 0.f}, {0.f, 2.f}};
		Point2 previous[] = {{3.f, -3.f}, {3.f, 1.f}, {-1.f, -1.f}};
		const auto fit = FitSimilarity({previous, 3}, {current, 3});
		bool roundTrip = fit.valid;
		for (int i = 0; i < 3; ++i)
		{
			const auto value = fit.Apply(current[i]);
			roundTrip = roundTrip && Near(value.x, previous[i].x) && Near(value.y, previous[i].y);
		}
		suite.Expect(roundTrip, "rigid similarity fit round-trip");
	}
	{
		const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0,
			0, 0, 1, 0, 0, 0, 0, 1};
		float translated[16];
		std::copy(std::begin(identity), std::end(identity), translated);
		translated[12] = 4.f;
		translated[13] = -3.f;
		const auto projected = ProjectNaomi2(translated, identity, identity, {2.f, 5.f, 1.f});
		suite.Expect(Near(projected.x, 6.f) && Near(projected.y, 2.f),
			"Naomi 2 exact matrix path follows column-major shader transform");
	}
	{
		const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0,
			0, 0, 1, 0, 0, 0, 0, 1};
		rend_context context{};
		context.framebufferWidth = 320;
		context.framebufferHeight = 240;
		context.verts.resize(3);
		context.verts[0].x = -1.f; context.verts[0].y = -1.f; context.verts[0].z = 1.f;
		context.verts[1].x = 1.f; context.verts[1].y = -1.f; context.verts[1].z = 1.f;
		context.verts[2].x = 0.f; context.verts[2].y = 1.f; context.verts[2].z = 1.f;
		context.idx = {0, 1, 2};
		context.matrices.resize(3);
		for (auto& matrix : context.matrices)
			std::copy(std::begin(identity), std::end(identity), matrix.mat);
		PolyParam poly{};
		poly.init();
		poly.first = 0;
		poly.count = 3;
		poly.tcw.full = 91;
		poly.mvMatrix = 0;
		poly.normalMatrix = 1;
		poly.projMatrix = 2;
		context.global_param_op.push_back(poly);
		RenderPass pass{};
		pass.op_count = 1;
		context.render_passes.push_back(pass);
		auto instrumentation = std::make_unique<NeuralInstrumentation>();
		instrumentation->SetEnabled(true);
		const auto& first = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		instrumentation->MarkEvaluated(first.frameId);
		context.matrices[0].mat[12] = 4.f;
		context.matrices[0].mat[13] = -3.f;
		const auto& moved = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		const auto *previousTransform =
			instrumentation->PreviousNaomi2TransformForOrdinal(0);
		const auto previousPositions = instrumentation->PreviousPositions();
		const auto previousProjected = previousTransform
			? ProjectNaomi2(previousTransform->modelView.data(),
				previousTransform->projection.data(), identity, {-1.f, -1.f, 1.f})
			: Point2{};
		const auto currentProjected = ProjectNaomi2(context.matrices[0].mat,
			context.matrices[2].mat, identity, {-1.f, -1.f, 1.f});
		suite.Expect(moved.matches.data[0].confidence >= .5f && previousTransform
			&& Near(previousTransform->modelView[12], 0.f)
			&& previousPositions.size == 3 && previousPositions.data[0].valid == 1.f
			&& Near(currentProjected.x - previousProjected.x, 4.f)
			&& Near(currentProjected.y - previousProjected.y, -3.f),
			"accepted Naomi 2 matrices and exact topology produce analytic prior motion");
		context.idx = {0, 2, 1};
		const auto& reindexed = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		suite.Expect(reindexed.matches.data[0].confidence == 0.f
			&& instrumentation->PreviousNaomi2TransformForOrdinal(0) == nullptr
			&& instrumentation->TrustedPreviousVertexCount() == 0,
			"reindexed Naomi 2 geometry remains current-color protected");
	}
	{
		DrawMatch unmatched{};
		const auto rejected = ClassifyMotion(unmatched, {40.f, -7.f}, false, false);
		DrawMatch exact{};
		exact.confidence = 1.f;
		exact.reason = static_cast<std::uint8_t>(MatchReason::Exact);
		const auto trusted = ClassifyMotion(exact, {4.f, 0.f}, false, false);
		const auto tooLarge = ClassifyMotion(exact, {129.f, 0.f}, false, false);
		suite.Expect(!rejected.trusted && rejected.biasCurrentColor && Near(rejected.motion.x, 0.f),
			"unmatched draw emits zero motion and current-color bias");
		suite.Expect(trusted.trusted && !trusted.biasCurrentColor && Near(trusted.motion.x, 4.f),
			"trusted motion survives classification");
		suite.Expect(!tooLarge.trusted && tooLarge.biasCurrentColor && Near(tooLarge.motion.x, 0.f),
			"motion above 128 native pixels is rejected");
	}
	{
		HistoryTracker history;
		suite.Expect(history.ConsumeReset() && !history.ConsumeReset(), "initial reset consumed once");
		history.Evaluated(17);
		history.Skipped();
		suite.Expect(history.EvaluatedFrameId() == 17 && history.ConsecutiveSkips() == 1 &&
			history.ConsumeReset() && !history.ConsumeReset(), "skip preserves reference and requests one reset");
		history.Discontinuity();
		suite.Expect(history.Generation() == 1 && history.ConsumeReset(), "discontinuity increments generation");
	}
	{
		PresentationCadence cadence;
		cadence.Observe(10, 10, 10, true);
		cadence.Observe(11, 11, 11, true);
		cadence.Observe(12, 0, 11, true);
		cadence.Observe(13, 13, 0, true);
		cadence.Observe(14, 14, 14, true);
		cadence.Observe(15, 15, 0, false);
		const auto& stats = cadence.Stats();
		suite.Expect(stats.observedPresents == 5 && stats.missingPresents == 1
			&& stats.acceptedEvaluations == 5 && stats.neuralPresents == 4
			&& stats.nativePresents == 1 && stats.acceptedNotPresented == 2
			&& stats.outputFrameRepeats == 1 && stats.sourceFrameRepeats == 0
			&& stats.nativeNeuralAlternations == 2 && stats.latencySamples == 4
			&& stats.latencyFramesTotal == 1 && stats.latencyFramesMax == 1
			&& stats.frameIdentityMismatches == 0,
			"presentation cadence counts accepted drops, repeats, alternation, and latency");
	}
	{
		RecoveryController recovery;
		recovery.SetReady();
		recovery.RecordTransientFailure(1, 100);
		recovery.RecordTransientFailure(30, 200);
		recovery.RecordTransientFailure(60, 300);
		suite.Expect(recovery.State() == RecoveryState::FallbackHold && recovery.HoldEntries() == 1,
			"three failures in sliding window enter hold once");
		for (int i = 0; i < 60; ++i) recovery.OnHostPresent();
		suite.Expect(!recovery.CanEvaluate(1299), "hold waits at least one second");
		suite.Expect(recovery.CanEvaluate(1300) && recovery.ConsumeResumeReset() &&
			!recovery.ConsumeResumeReset(), "hold resumes with exactly one reset");
		recovery.RecordTransientFailure(200, 1400);
		suite.Expect(recovery.State() == RecoveryState::Ready,
			"hold exit clears stale failure window");
		RecoveryController removed;
		removed.SetReady();
		removed.DeviceRemoved();
		for (int i = 0; i < 120; ++i) removed.OnHostPresent();
		suite.Expect(!removed.CanEvaluate(10000)
			&& removed.State() == RecoveryState::DeviceRemoved,
			"device-removed state stays on native fallback until stage recreation");
	}
	{
		StageConfig config;
		config.mode = NeuralMode::Passthrough;
		NeuralStage stage(config);
		std::uint32_t identity = 1;
		NeuralFrame frame;
		frame.frameId = 9;
		frame.color.api = TextureApi::D3D11;
		frame.color.resource = &identity;
		suite.Expect(stage.TrySubmit(frame) == SubmitStatus::Submitted &&
			stage.TrySubmit(frame) == SubmitStatus::Submitted && stage.GetStats().submissions == 1,
			"stage evaluates an emulated frame once");
		frame.frameId = 10;
		frame.source = FrameSource::FramebufferDirect;
		suite.Expect(stage.TrySubmit(frame) == SubmitStatus::Unsupported,
			"framebuffer-direct bypasses stage");
	}
	{
		Dlss5CompatibilityRebuildPolicy policy;
		policy.Configure(3, 2);
		policy.Observe(Dlss5HookReadiness::MissingComponents, 20);
		policy.Observe(Dlss5HookReadiness::ComponentsPresent, 20);
		policy.Observe(Dlss5HookReadiness::ContractEvaluated, 22);
		const bool beforeGrace = !policy.ConsumeReleaseRequest();
		policy.Observe(Dlss5HookReadiness::ContractEvaluated, 23);
		const bool firstRelease = policy.ConsumeReleaseRequest()
			&& !policy.ConsumeReleaseRequest() && policy.BeginCreateAttempt();
		policy.CompleteCreateAttempt(false);
		const bool retry = policy.RetryAvailable() && policy.BeginCreateAttempt()
			&& policy.LastReason() == Dlss5RebuildReason::RetryAfterCreateFailure;
		policy.CompleteCreateAttempt(true);
		suite.Expect(beforeGrace && firstRelease && retry
			&& policy.Attempts() == 2 && policy.Failures() == 1
			&& policy.SuccessfulRebuilds() == 1 && !policy.RecreatePending(),
			"DLSS 5 readiness transition debounces one rebuild and bounds its retry");

		Dlss5CompatibilityRebuildPolicy disabled;
		disabled.Configure(0, 0);
		disabled.Observe(Dlss5HookReadiness::ComponentsPresent, 0);
		suite.Expect(!disabled.ConsumeReleaseRequest() && disabled.Attempts() == 0,
			"DLSS 5 compatibility rebuild can be disabled by configuration");
	}
	{
		auto experimental = CreateNeuralBackend(NeuralMode::Dlss5Experimental, Api::D3D12);
		suite.Expect(experimental->Initialize({}, nullptr, nullptr) == BackendEvalStatus::Unsupported
			&& std::string(experimental->GetStatusReason()).find(
#ifdef FLYCAST_ENABLE_NGX
				"D3D12 device"
#else
				"D3D11On12"
#endif
			) != std::string::npos,
			"experimental DLSS 5 D3D12 candidate reports an uninitialized public-NGX seam");
		Dlss5HookComponents complete{true, true, true, true};
		const auto missingRoute = AssessDlss5Hook(true, Dlss5HookRoute::None, complete, false);
		const auto missingComponents = AssessDlss5Hook(true, Dlss5HookRoute::D3D11External, {}, false);
		const auto ready = AssessDlss5Hook(true, Dlss5HookRoute::D3D11External, complete, false);
		const auto evaluated = AssessDlss5Hook(true, Dlss5HookRoute::D3D11On12, complete, true);
		suite.Expect(!missingRoute.componentsPresent
			&& missingRoute.readiness == Dlss5HookReadiness::MissingRoute
			&& !missingComponents.componentsPresent
			&& missingComponents.readiness == Dlss5HookReadiness::MissingComponents
			&& ready.componentsPresent && ready.readiness == Dlss5HookReadiness::ComponentsPresent
			&& evaluated.componentsPresent && evaluated.readiness == Dlss5HookReadiness::ContractEvaluated,
			"DLSS 5 readiness distinguishes route, components, and evaluated contract");
		auto experimentalD3D11 = CreateNeuralBackend(NeuralMode::Dlss5Experimental, Api::D3D11);
		suite.Expect(experimentalD3D11->Initialize({}, nullptr, nullptr) == BackendEvalStatus::Unsupported
			&& std::string(experimentalD3D11->GetStatusReason()).find(
#ifdef FLYCAST_ENABLE_NGX
				"D3D11 device"
#else
				"without FLYCAST_NEURAL_NGX"
#endif
			) != std::string::npos,
			"experimental DLSS 5 accepts the D3D11 public-NGX candidate route");
		auto d3d12 = CreateNeuralBackend(NeuralMode::DlaaHook, Api::D3D12);
		suite.Expect(d3d12->Initialize({}, nullptr, nullptr) == BackendEvalStatus::Unsupported
			&& std::string(d3d12->GetStatusReason()).find(
#ifdef FLYCAST_ENABLE_NGX
				"D3D12 device"
#else
				"D3D11On12"
#endif
			) != std::string::npos,
			"D3D12 backend reports the uninitialized surface precisely");
	}
	{
		rend_context context{};
		context.framebufferWidth = 320;
		context.framebufferHeight = 240;
		context.verts.resize(3);
		context.verts[0].x = 10; context.verts[0].y = 20; context.verts[0].z = .2f;
		context.verts[1].x = 80; context.verts[1].y = 25; context.verts[1].z = .3f;
		context.verts[2].x = 40; context.verts[2].y = 90; context.verts[2].z = .4f;
		context.idx = {0, 1, 2};
		PolyParam poly{};
		poly.init();
		poly.first = 0;
		poly.count = 3;
		poly.tcw.full = 55;
		context.global_param_op.push_back(poly);
		RenderPass pass{};
		pass.op_count = 1;
		context.render_passes.push_back(pass);
		auto instrumentation = std::make_unique<NeuralInstrumentation>();
		instrumentation->SetEnabled(true);
		const auto& first = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		const bool firstReset = first.resetHistory;
		const auto firstDrawCount = first.draws.size;
		const auto firstHash = instrumentation->DrawSnapshotHash();
		const auto firstFrameId = first.frameId;
		instrumentation->MarkEvaluated(firstFrameId);
		const auto& second = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		suite.Expect(firstDrawCount == 1 && firstReset,
			"rend_context snapshot emits first-frame reset");
		suite.Expect(second.draws.size == 1 && second.matches.data[0].tier == 1 &&
			second.historyValid && instrumentation->DrawSnapshotHash() == firstHash,
			"rend_context snapshot and draw hash are deterministic");
		suite.Expect(!second.truncated, "atomic frame carries draw-overflow state");
		std::uint32_t resources[6]{};
		const TextureRef refs[] = {
			{TextureApi::D3D11, &resources[0], nullptr, 28},
			{TextureApi::D3D11, &resources[1], nullptr, 41},
			{TextureApi::D3D11, &resources[2], nullptr, 34},
			{TextureApi::D3D11, &resources[3], nullptr, 61},
			{TextureApi::D3D11, &resources[4], nullptr, 61},
			{TextureApi::D3D11, &resources[5], nullptr, 57},
		};
		const auto& attached = instrumentation->AttachTextures(refs[0], refs[1], refs[2],
			refs[3], refs[4], refs[5]);
		suite.Expect(attached.color.resource == &resources[0]
			&& attached.depth.resource == &resources[1]
			&& attached.motion.resource == &resources[2]
			&& attached.mask.resource == &resources[3]
			&& attached.confidence.resource == &resources[4]
			&& attached.drawId.resource == &resources[5]
			&& attached.frameId == second.frameId,
			"atomic frame attaches the complete GPU export set");
		context.global_param_op[0].tcw.full = 56;
		const auto& skipped = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		suite.Expect(skipped.matches.data[0].confidence == 0.f && skipped.sceneCut
			&& skipped.resetHistory,
			"unmatched scene cut rejects motion and resets history");
		context.global_param_op[0].tcw.full = 55;
		const auto& afterSkip = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		suite.Expect(afterSkip.matches.data[0].tier == 1,
			"matching uses last successfully evaluated draw history");
		const auto generation = afterSkip.historyGeneration;
		const auto& direct = instrumentation->CaptureSource(FrameSource::FramebufferDirect,
			{}, 320, 240, 320, 240, {0, 0, 320, 240});
		suite.Expect(direct.source == FrameSource::FramebufferDirect && direct.draws.empty() &&
			direct.resetHistory && direct.historyGeneration == generation + 1,
			"framebuffer-direct package is geometry-free and resets history");
		const auto& geometryAgain = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		suite.Expect(geometryAgain.resetHistory &&
			geometryAgain.historyGeneration == generation + 2,
			"framebuffer-direct to geometry transition increments history generation");
	}
	{
		rend_context context{};
		context.framebufferWidth = 320;
		context.framebufferHeight = 240;
		context.verts.resize(3);
		context.verts[0].x = 10; context.verts[0].y = 20; context.verts[0].z = .2f;
		context.verts[1].x = 80; context.verts[1].y = 25; context.verts[1].z = .3f;
		context.verts[2].x = 40; context.verts[2].y = 90; context.verts[2].z = .4f;
		context.idx = {0, 1, 2};
		PolyParam poly{};
		poly.init();
		poly.first = 0;
		poly.count = 3;
		poly.tcw.full = 55;
		context.global_param_op.push_back(poly);
		RenderPass pass{};
		pass.op_count = 1;
		context.render_passes.push_back(pass);
		auto instrumentation = std::make_unique<NeuralInstrumentation>();
		instrumentation->SetEnabled(true);
		const auto& original = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		const auto originalFrame = original.frameId;
		const auto originalStructure = DrawStructuralSignature(original.draws.data[0]);
		instrumentation->MarkEvaluated(originalFrame);
		for (auto& vertex : context.verts) { vertex.x += 24.f; vertex.y -= 7.f; }
		const auto& moved = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		const auto previousPositions = instrumentation->PreviousPositions();
		const float movedConfidence = moved.matches.data[0].confidence;
		suite.Expect(moved.matches.data[0].tier == 1 &&
			DrawStructuralSignature(moved.draws.data[0]) == originalStructure &&
			moved.draws.data[0].centroid[0] > 40.f,
			"production draw capture retains identity across pose translation");
		suite.Expect(previousPositions.size == 3 &&
			instrumentation->TrustedPreviousVertexCount() == 3 &&
			previousPositions.data[0].valid == 1.f && previousPositions.data[0].x == 10.f &&
			previousPositions.data[1].valid == 1.f && previousPositions.data[1].x == 80.f &&
			previousPositions.data[2].valid == 1.f && previousPositions.data[2].x == 40.f,
			"accepted exact topology emits prior positions by strip index");
		for (auto& vertex : context.verts) vertex.x += 6.f;
		const auto& afterOneSkip = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		const auto afterSkippedPositions = instrumentation->PreviousPositions();
		const float oneSkipConfidence = afterOneSkip.matches.data[0].confidence;
		suite.Expect(afterOneSkip.historyAge == 2 && afterOneSkip.skippedFrameCount == 1
			&& oneSkipConfidence >= .5f && oneSkipConfidence < movedConfidence
			&& afterSkippedPositions.size == 3 &&
			afterSkippedPositions.data[0].x == 10.f &&
			afterSkippedPositions.data[1].x == 80.f &&
			afterSkippedPositions.data[2].x == 40.f,
			"one skipped pose ages confidence without replacing accepted history");
		const auto& afterTwoSkips = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		const bool twoSkipsRetainBoundedTrust = afterTwoSkips.historyAge == 3
			&& afterTwoSkips.skippedFrameCount == 2
			&& afterTwoSkips.matches.data[0].confidence >= .5f
			&& afterTwoSkips.matches.data[0].confidence < oneSkipConfidence;
		const auto& afterThreeSkips = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		suite.Expect(twoSkipsRetainBoundedTrust && afterThreeSkips.historyAge == 4
			&& afterThreeSkips.skippedFrameCount == 3
			&& afterThreeSkips.matches.data[0].confidence == 0.f
			&& instrumentation->TrustedPreviousVertexCount() == 0
			&& std::all_of(instrumentation->PreviousPositions().begin(),
				instrumentation->PreviousPositions().end(),
				[](const PreviousPosition& position) { return position.valid == 0.f; }),
			"history age degrades then rejects confidence after three skipped frames");
	}
	{
		rend_context context{};
		context.framebufferWidth = 320;
		context.framebufferHeight = 240;
		context.verts.resize(3);
		context.verts[0].x = 10; context.verts[0].y = 20; context.verts[0].z = .2f;
		context.verts[1].x = 80; context.verts[1].y = 25; context.verts[1].z = .3f;
		context.verts[2].x = 40; context.verts[2].y = 90; context.verts[2].z = .4f;
		context.idx = {0, 1, 2};
		PolyParam poly{};
		poly.init();
		poly.first = 0;
		poly.count = 3;
		poly.tcw.full = 63;
		context.global_param_op.push_back(poly);
		RenderPass pass{};
		pass.op_count = 1;
		context.render_passes.push_back(pass);
		auto instrumentation = std::make_unique<NeuralInstrumentation>();
		instrumentation->SetEnabled(true);
		const auto& original = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		instrumentation->MarkEvaluated(original.frameId);
		for (auto& vertex : context.verts) { vertex.x += 12.f; vertex.y -= 4.f; }
		context.idx = {0, 2, 1};
		const auto& reindexed = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		const auto fittedPositions = instrumentation->PreviousPositions();
		suite.Expect(reindexed.matches.data[0].tier == 2
			&& reindexed.matches.data[0].confidence >= .5f
			&& Near(reindexed.matches.data[0].fitResidual, 0.f, .001f)
			&& instrumentation->TrustedPreviousVertexCount() == 3
			&& fittedPositions.data[0].valid == 1.f && Near(fittedPositions.data[0].x, 10.f)
			&& fittedPositions.data[1].valid == 1.f && Near(fittedPositions.data[1].x, 80.f)
			&& fittedPositions.data[2].valid == 1.f && Near(fittedPositions.data[2].x, 40.f),
			"reindexed rigid geometry uses bounded low-residual similarity fit");
		context.verts[2].x += 25.f;
		const auto& deformed = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		suite.Expect(deformed.matches.data[0].tier != 0
			&& deformed.matches.data[0].fitResidual > .25f
			&& deformed.matches.data[0].confidence == 0.f
			&& instrumentation->TrustedPreviousVertexCount() == 0
			&& std::all_of(instrumentation->PreviousPositions().begin(),
				instrumentation->PreviousPositions().end(),
				[](const PreviousPosition& position) { return position.valid == 0.f; }),
			"reindexed deformation above residual threshold rejects false motion");
	}
	{
		rend_context context{};
		context.framebufferWidth = 320;
		context.framebufferHeight = 240;
		context.verts.resize(6);
		for (int i = 0; i < 3; ++i)
		{
			context.verts[i].x = static_cast<float>(i * 10);
			context.verts[i].y = static_cast<float>(i * 5);
			context.verts[i].z = .5f;
			context.verts[i + 3] = context.verts[i];
			context.verts[i + 3].x += 100.f;
		}
		context.idx = {0, 1, 2, 3, 4, 5};
		PolyParam first{};
		first.init();
		first.first = 0;
		first.count = 3;
		first.tcw.full = 71;
		PolyParam second = first;
		second.first = 3;
		context.global_param_op = {first, second};
		RenderPass pass{};
		pass.op_count = 2;
		context.render_passes.push_back(pass);
		auto instrumentation = std::make_unique<NeuralInstrumentation>();
		instrumentation->SetEnabled(true);
		const auto& previous = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		instrumentation->MarkEvaluated(previous.frameId);
		context.idx = {0, 1, 2, 0, 1, 2};
		instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		suite.Expect(instrumentation->TrustedPreviousVertexCount() == 0 &&
			instrumentation->MatchForOrdinal(0)->confidence == 0.f &&
			instrumentation->MatchForOrdinal(1)->confidence == 0.f &&
			std::all_of(instrumentation->PreviousPositions().begin(),
				instrumentation->PreviousPositions().end(),
				[](const PreviousPosition& position) { return position.valid == 0.f; }),
			"shared current vertices with conflicting history are invalidated");
	}
	{
		const auto jitter = HaltonJitter(0, 8);
		suite.Expect(Near(jitter.x, 0.f) && Near(jitter.y, -1.f / 6.f), "Halton first phase");
		suite.Expect(JitterPhaseCount(1920, 1920) == 8 && JitterPhaseCount(960, 1920) == 32,
			"jitter phase count");
	}
	suite.Expect(IsSceneCut(34, 100) && !IsSceneCut(35, 100), "scene-cut threshold");
	suite.Expect(NextHistorySafeRingSlot(0, 1, 3, false) == 1
		&& NextHistorySafeRingSlot(0, 1, 3, true) == 2
		&& NextHistorySafeRingSlot(2, 1, 3, true) == 0,
		"guidance ring cannot overwrite retained accepted history after skips");
	{
		const auto fourThree = ComputeContentRect(1920, 1080, 4.f / 3.f, false, 480);
		const auto widescreen = ComputeContentRect(1920, 1080, 16.f / 9.f, false, 480);
		suite.Expect(fourThree.x == 240 && fourThree.y == 0 && fourThree.width == 1440 &&
			fourThree.height == 1080 && widescreen.x == 0 && widescreen.width == 1920,
			"content rect respects 4:3 and widescreen aspect");
		const auto matchHd = ComputeMatchOutputRasterSize(1920, 1080, 4.f / 3.f, false);
		const auto matchQhd = ComputeMatchOutputRasterSize(2560, 1440, 4.f / 3.f, false);
		const auto matchUhd = ComputeMatchOutputRasterSize(3840, 2160, 4.f / 3.f, false);
		const auto matchWide = ComputeMatchOutputRasterSize(3840, 2160, 16.f / 9.f, false);
		suite.Expect(matchHd.width == 1440 && matchHd.height == 1080
			&& matchQhd.width == 1920 && matchQhd.height == 1440
			&& matchUhd.width == 2880 && matchUhd.height == 2160
			&& matchWide.width == 3840 && matchWide.height == 2160,
			"match-output raster uses exact post-aspect content dimensions");
		suite.Expect(RoundManualRasterWidth(640.f * (320.f / 480.f), false) == 426
			&& RoundManualRasterWidth(640.f * (320.f / 480.f), true) == 427,
			"Quality SR exact-width path avoids the one-pixel NGX contract mismatch");
		suite.Expect(UsesMatchOutputRaster(2) && UsesMatchOutputRaster(3)
			&& UsesMatchOutputRaster(8) && !UsesMatchOutputRaster(0)
			&& !UsesMatchOutputRaster(1) && !UsesMatchOutputRaster(4),
			"match-output raster is limited to target-native lanes");
	}
	{
		const float sourceDepth = .375f;
		const float legacyEncoded = std::log2(1.f + 100000.f * sourceDepth) / 34.f;
		const float nativeEncoded = std::log2(1.f + 100000.f / sourceDepth) / 34.f;
		suite.Expect(Near(InvertLegacyDepth(legacyEncoded, false), sourceDepth, 1e-3f) &&
			Near(InvertLegacyDepth(nativeEncoded, true), sourceDepth, 1e-3f),
			"depth visualization inverse covers legacy and DIV_POS_Z paths");
	}
	{
		DepthContractResult native;
		DepthContractResult on12;
		std::string fixtureError;
		const bool nativeOk = RunDepthContractFixture(false, native, fixtureError);
		suite.Expect(nativeOk, "production depth contract passes on native D3D11");
		fixtureError.clear();
		const bool on12Ok = RunDepthContractFixture(true, on12, fixtureError);
		suite.Expect(on12Ok, "production depth contract passes on D3D11On12");
		suite.Expect(nativeOk && on12Ok && native.correctDepth == on12.correctDepth
			&& native.correctColor.rgba == on12.correctColor.rgba,
			"production depth contract is exact across D3D11 surfaces");
	}
	{
		MotionContractResult motion;
		std::string fixtureError;
		const bool motionOk = RunMotionContractFixture(motion, fixtureError);
		suite.Expect(motionOk && motion.analyticTruth,
			"GPU motion contract matches analytic render-pixel truth");
		suite.Expect(motionOk && motion.negativeControlsFail,
			"reversed and doubled motion fail pixel reprojection");
	}
	{
		ProductionMotionResult native;
		ProductionMotionResult on12;
		std::string fixtureError;
		const bool nativeOk = RunProductionMotionFixture(false, native, fixtureError);
		if (!nativeOk && !fixtureError.empty()) std::cerr << fixtureError << '\n';
		suite.Expect(nativeOk && native.analyticTruth,
			"production PVR shader rasterizes accepted motion on native D3D11");
		fixtureError.clear();
		const bool on12Ok = RunProductionMotionFixture(true, on12, fixtureError);
		if (!on12Ok && !fixtureError.empty()) std::cerr << fixtureError << '\n';
		suite.Expect(on12Ok && on12.analyticTruth,
			"production PVR shader rasterizes accepted motion on D3D11On12");
		suite.Expect(nativeOk && on12Ok && Near(native.trustedX, on12.trustedX)
			&& Near(native.trustedY, on12.trustedY)
			&& native.trustedMask == on12.trustedMask
			&& native.trustedConfidence == on12.trustedConfidence
			&& native.trustedDrawId == on12.trustedDrawId
			&& native.trustedPreviousDrawId == on12.trustedPreviousDrawId,
			"production motion guidance is exact across D3D11 surfaces");
		suite.Expect(nativeOk && on12Ok && native.invalidProtected && on12.invalidProtected
			&& native.magnitudeProtected && on12.magnitudeProtected,
			"invalid and excessive production motion are current-color protected");
		suite.Expect(nativeOk && on12Ok && native.naomi2AnalyticTruth
			&& on12.naomi2AnalyticTruth && Near(native.naomi2X, on12.naomi2X)
			&& Near(native.naomi2Y, on12.naomi2Y)
			&& native.naomi2Mask == on12.naomi2Mask
			&& native.naomi2Confidence == on12.naomi2Confidence,
			"accepted-history Naomi 2 HLSL motion is analytic and cross-surface exact");
		suite.Expect(nativeOk && on12Ok && native.naomi2InvalidProtected
			&& on12.naomi2InvalidProtected,
			"missing Naomi 2 matrix history remains current-color protected");
		suite.Expect(nativeOk && on12Ok && native.rasterJitterShiftedCoverage
			&& on12.rasterJitterShiftedCoverage && native.jitterExcludedFromMotion
			&& on12.jitterExcludedFromMotion,
			"production raster jitter shifts coverage but remains absent from motion");
		suite.Expect(nativeOk && on12Ok && native.oitRasterJitterShiftedCoverage
			&& on12.oitRasterJitterShiftedCoverage,
			"production OIT raster jitter shifts coverage exactly on both D3D11 surfaces");
		suite.Expect(nativeOk && on12Ok && native.naomi2RasterJitterShiftedCoverage
			&& on12.naomi2RasterJitterShiftedCoverage,
			"Naomi 2 production raster jitter shifts coverage without contaminating motion");
	}
	{
		ColorContractResult color;
		std::string fixtureError;
		const bool colorOk = RunColorContractFixture(color, fixtureError);
		suite.Expect(colorOk && color.byteExact && color.channelsExact
			&& color.grayscaleExact && color.alphaIndependent,
			"production SDR quad path preserves color and alpha exactly");
		suite.Expect(colorOk && color.contentRectsExact,
			"content rectangle examples and odd-size rounding are exact");
	}
	{
		DisocclusionContractResult native;
		DisocclusionContractResult on12;
		std::string fixtureError;
		const bool nativeOk = RunDisocclusionContractFixture(false, native, fixtureError);
		if (!nativeOk && !fixtureError.empty()) std::cerr << fixtureError << '\n';
		suite.Expect(nativeOk, "production disocclusion contract passes on native D3D11");
		fixtureError.clear();
		const bool on12Ok = RunDisocclusionContractFixture(true, on12, fixtureError);
		if (!on12Ok && !fixtureError.empty()) std::cerr << fixtureError << '\n';
		suite.Expect(on12Ok, "production disocclusion contract passes on D3D11On12");
		suite.Expect(nativeOk && on12Ok
			&& native.resolvedMask.rgba == on12.resolvedMask.rgba,
			"production disocclusion mask is exact across D3D11 surfaces");
		suite.Expect(nativeOk && on12Ok && native.wrongMissedPixels > 0
			&& native.wrongTrailEnergy > native.correctTrailEnergy,
			"wrong disocclusion control has measurable trail energy");
	}
	{
		TransparencyContractResult native;
		TransparencyContractResult on12;
		std::string fixtureError;
		const bool nativeOk = RunTransparencyContractFixture(false, native, fixtureError);
		if (!nativeOk && !fixtureError.empty()) std::cerr << fixtureError << '\n';
		suite.Expect(nativeOk, "production OIT visible fragments are reactive on native D3D11");
		fixtureError.clear();
		const bool on12Ok = RunTransparencyContractFixture(true, on12, fixtureError);
		if (!on12Ok && !fixtureError.empty()) std::cerr << fixtureError << '\n';
		suite.Expect(on12Ok, "production OIT visible fragments are reactive on D3D11On12");
		suite.Expect(nativeOk && on12Ok
			&& native.reactiveMask.rgba == on12.reactiveMask.rgba,
			"production OIT reactive coverage is exact across D3D11 surfaces");
		suite.Expect(nativeOk && on12Ok && native.emptyAndModifierClear
			&& native.singleLayerReactive && native.multiLayerReactive
			&& native.wrongControlFailed && native.mergePreservesBase,
			"empty/modifier-only pixels stay clear while single and multi-layer translucency reject the omitted-mask control");
	}
	{
		OverlayContractResult native;
		OverlayContractResult on12;
		std::string fixtureError;
		const bool nativeOk = RunOverlayContractFixture(false, native, fixtureError);
		if (!nativeOk && !fixtureError.empty()) std::cerr << fixtureError << '\n';
		suite.Expect(nativeOk, "protected overlay pixels composite byte-exactly on native D3D11");
		fixtureError.clear();
		const bool on12Ok = RunOverlayContractFixture(true, on12, fixtureError);
		if (!on12Ok && !fixtureError.empty()) std::cerr << fixtureError << '\n';
		suite.Expect(on12Ok, "protected overlay pixels composite byte-exactly on D3D11On12");
		suite.Expect(nativeOk && on12Ok && native.composited.rgba == on12.composited.rgba,
			"overlay composition is exact across D3D11 surfaces");
		suite.Expect(nativeOk && on12Ok && native.worldChanged == 0
			&& native.wrongProtectedMismatch == native.protectedPixels,
			"default overlay composite preserves unclassified world and omitted-composite control fails");
	}

	for (const auto& test : CaptureComparisonSelfTests())
		suite.Expect(test.second, test.first);
	const auto remakeCounts = remake::TestSceneContract();
	{
		rend_context ctx{};
		ctx.verts.resize(3); ctx.idx = {0,1,2};
		PolyParam poly{}; poly.init(); poly.count=3; ctx.global_param_op.push_back(poly);
		RenderPass pass{};pass.op_count=1;ctx.render_passes.push_back(pass);
		ctx.verts[1].x=1.25f;ctx.verts[1].u=-0.5f;ctx.verts[2].col[2]=197;
		std::array<float,16> viewport{}; viewport[0]=viewport[5]=viewport[10]=viewport[15]=1;
		const auto path=std::filesystem::temp_directory_path() / ("flycast-pvr-packet-"
			+std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count())+".json");
		std::string error;
		suite.Expect(WritePvrScenePacket(path,ctx,viewport,7,"fixture",error),"PVR packet bounded export");
		PvrDecodedPacket live;
		suite.Expect(SnapshotPvrScenePacket(ctx,viewport,7,"fixture",live,error)
			&&live.frame==7&&live.vertices.size()==3&&live.draws.size()==1
			&&!live.draws[0].state.texture&&live.indices==ctx.idx,"PVR live snapshot owned metadata");
		ctx.verts[1].x=9;
		suite.Expect(live.vertices[1].x==1.25f,"PVR snapshot survives source mutation");
		ctx.verts[1].x=1.25f;
#ifdef FLYCAST_ENABLE_NEURAL
		ctx.captureProducer={2,7,900};
		SourceVertexObservation joined; joined.child=3; joined.copy.decodedVertex=1;
		std::memcpy(joined.copy.after.data()+1,&ctx.verts[1].x,3*sizeof(float));
		joined.copy.before=joined.copy.after;ctx.sourceVertices.push_back(joined);
		suite.Expect(SnapshotPvrScenePacket(ctx,viewport,7,"fixture",live,error)
			&&live.sourceVertices.size()==1&&live.sourceVertices[0].child==3
			&&live.sourceProducer.ordinal==7,"PVR snapshot retains child-qualified live source join");
		ctx.sourceVertices[0].copy.decodedVertex=2;
		suite.Expect(!SnapshotPvrScenePacket(ctx,viewport,8,"fixture",live,error)
			&&live.sourceVertices[0].copy.decodedVertex==1,"PVR wrong vertex join rejects without replacing snapshot");
		ctx.sourceVertices.clear();ctx.captureProducer={};
		suite.Expect(live.sourceVertices.size()==1,"PVR source join survives producer retirement");
#endif
		ctx.isRTT=true;
		suite.Expect(!SnapshotPvrScenePacket(ctx,viewport,8,"fixture",live,error)&&live.frame==7,
			"PVR snapshot RTT rejection preserves previous output");
		ctx.isRTT=false;ctx.idx[0]=99;
		suite.Expect(!SnapshotPvrScenePacket(ctx,viewport,8,"fixture",live,error)&&live.frame==7,
			"PVR snapshot invalid index rejected atomically");
		ctx.idx[0]=0;
		const auto savedWidth=ctx.framebufferWidth;ctx.framebufferWidth=4097;
		suite.Expect(!SnapshotPvrScenePacket(ctx,viewport,8,"fixture",live,error)&&live.frame==7,
			"PVR snapshot framebuffer bound preserves prior output");
		ctx.framebufferWidth=savedWidth;
		ctx.verts[0].x=std::numeric_limits<float>::quiet_NaN();
		suite.Expect(!SnapshotPvrScenePacket(ctx,viewport,8,"fixture",live,error)&&live.frame==7,
			"PVR snapshot referenced nonfinite rejected");
		ctx.verts[0].x=0;
		const auto savedPass=ctx.render_passes[0];ctx.render_passes[0].op_count=2;
		suite.Expect(!SnapshotPvrScenePacket(ctx,viewport,8,"fixture",live,error)&&live.frame==7,
			"PVR snapshot invalid pass coverage rejected");
		ctx.render_passes[0]=savedPass;
		suite.Expect(!SnapshotPvrScenePacket(ctx,viewport,0,"fixture",live,error)&&live.frame==7,
			"PVR snapshot zero frame rejected");
		std::ifstream file(path,std::ios::binary);
		const std::string contents((std::istreambuf_iterator<char>(file)),{}); file.close();
		suite.Expect(contents.find("\"camera_provenance\":\"unknown\"")!=std::string::npos
			&&contents.find("1065353216")!=std::string::npos,"PVR packet retains unknown camera and exact float bits");
		PvrDecodedPacket decoded;
		const bool loaded=ReadPvrScenePacket(path,7,"fixture",decoded,error);
		suite.Expect(loaded&&decoded.vertices.size()==3&&decoded.indices==ctx.idx
			&&std::memcmp(decoded.vertices.data(),ctx.verts.data(),3*sizeof(::Vertex))==0
			&&decoded.viewport==viewport&&decoded.draws.size()==1&&!decoded.draws[0].state.texture
			&&decoded.passes.size()==1&&decoded.omissions.size()==7&&decoded.sortedOrderCaptured,"PVR disk decoder preserves exact vertex bytes and unresolved state");
		suite.Expect(!ReadPvrScenePacket(path,8,"fixture",decoded,error)
			&&error=="pvr-decode-frame-game-mismatch"&&decoded.frame==7,"PVR wrong-frame decode preserves prior output");
		const auto golden=nlohmann::json::parse(contents);
		auto reject=[&](nlohmann::json bad,const char* reason,const char* name) {
			{std::ofstream f(path,std::ios::binary);f<<bad.dump();}
			suite.Expect(!ReadPvrScenePacket(path,7,"fixture",decoded,error)&&error==reason&&decoded.frame==7,name);
		};
		auto bad=golden;bad["indices"][0]=-1;
		reject(bad,"pvr-decode-unsigned-integer","PVR decoder rejects signed index");
		bad=golden;bad["indices"][0]=4294967296ull;
		reject(bad,"pvr-decode-u32-range","PVR decoder rejects integer narrowing");
		bad=golden;bad["indices"][0]=3;
		reject(bad,"pvr-decode-index-range","PVR decoder rejects out-of-range index");
		bad=golden;bad["vertices"][0][5][0]=256;
		reject(bad,"pvr-decode-color-range","PVR decoder rejects color narrowing");
		bad=golden;bad["camera_provenance"]="supplied";
		reject(bad,"pvr-decode-provenance","PVR decoder rejects silently upgraded camera");
		bad=golden;bad["omissions"]=nlohmann::json::array();
		reject(bad,"pvr-decode-omissions","PVR decoder rejects hidden omissions");
		bad=golden;bad["viewport_bits"][0]=2139095040u;
		reject(bad,"pvr-decode-viewport","PVR decoder rejects nonfinite viewport");
		bad=golden;bad["vertices"][0][2]=2139095040u;bad["nonfinite_position_count"]=1;
		reject(bad,"pvr-decode-referenced-nonfinite","PVR decoder rejects referenced nonfinite geometry");
		bad=golden;bad["passes"][0]["op"]=2;
		reject(bad,"pvr-decode-pass-range","PVR decoder rejects pass overrun");
		bad=golden;bad["draws"][0]["texture"]={{"upload_generation",1u},{"palette_hash",123u},{"rtt_generation",0u}};
		reject(bad,"pvr-decode-palette-applicability","PVR decoder rejects palette metadata on RGB texture");
		bad=golden;bad["draws"][0]["count"]=4u;
		reject(bad,"pvr-decode-draw-range","PVR decoder rejects overflowing draw range");
		bad=golden;bad["draws"][0]["first"]=0xffffffffu;bad["draws"][0]["count"]=0u;
		{std::ofstream f(path,std::ios::binary);f<<bad.dump();}
		suite.Expect(ReadPvrScenePacket(path,7,"fixture",decoded,error)
			&&decoded.draws[0].state.first==0xffffffffu&&decoded.draws[0].state.count==0,
			"PVR decoder preserves dormant empty draw offset without indexing it");
		bad["draws"][0]["count"]=1u;
		reject(bad,"pvr-decode-draw-range","PVR dormant offset cannot become a nonempty invalid draw");
		bad=golden;bad["naomi2_matrix_count"]=1u;
		reject(bad,"pvr-decode-naomi2-unsupported","PVR decoder rejects omitted Naomi2 transforms");
		{std::ofstream f(path,std::ios::binary);f<<"{\"a\":1,\"a\":2}";}
		suite.Expect(!ReadPvrScenePacket(path,7,"fixture",decoded,error)&&error=="pvr-decode-duplicate-key","PVR decoder rejects duplicate keys");
		{std::ofstream f(path,std::ios::binary);f<<std::string(32,'[')<<"0"<<std::string(32,']');}
		suite.Expect(!ReadPvrScenePacket(path,7,"fixture",decoded,error)&&error=="pvr-decode-parser-budget","PVR decoder limits nesting before DOM growth");
		{std::ofstream f(path,std::ios::binary);f.seekp(32*1024*1024);f.put('x');}
		suite.Expect(!ReadPvrScenePacket(path,7,"fixture",decoded,error)&&error=="pvr-decode-byte-bound","PVR decoder limits bytes before allocation");
		std::filesystem::remove(path);
		ctx.global_param_op[0].first=0xffffffffu;ctx.global_param_op[0].count=0;
		suite.Expect(WritePvrScenePacket(path,ctx,viewport,7,"fixture",error),"PVR writer retains zero-count dormant draw");
		suite.Expect(ReadPvrScenePacket(path,7,"fixture",decoded,error)
			&&decoded.draws[0].state.first==0xffffffffu&&decoded.draws[0].state.count==0,
			"PVR dormant empty draw roundtrip is exact");
		std::filesystem::remove(path);ctx.global_param_op[0].count=1;
		suite.Expect(!WritePvrScenePacket(path,ctx,viewport,7,"fixture",error)
			&&error=="pvr-packet-draw-range"&&!std::filesystem::exists(path),
			"PVR nonempty dormant offset rejected before output");
		ctx.global_param_op[0].first=0;ctx.global_param_op[0].count=3;
		// Triangle sorting preserves source vertex ranges in translucent PolyParam;
		// the actual GPU index ranges live in separate sorted commands.
		ctx.verts.resize(7);ctx.idx={0,1,2,4,5,6};
		PolyParam translucent{};translucent.init();translucent.first=4;translucent.count=3;
		ctx.global_param_tr.push_back(translucent);ctx.sortedTriangles.push_back({0,3,3});
		ctx.render_passes[0].tr_count=1;ctx.render_passes[0].sorted_tr_count=1;
		ctx.render_passes[0].autosort=true;
		const bool sortedWritten=WritePvrScenePacket(path,ctx,viewport,7,"fixture",error);
		suite.Expect(SnapshotPvrScenePacket(ctx,viewport,9,"fixture",live,error)
			&&live.draws[1].vertexRange&&live.sortedTriangles.size()==1
			&&live.sortedTriangles[0].first==3&&live.passes[0].autosort,
			"PVR snapshot owns sorted translucent range semantics");
		suite.Expect(sortedWritten,"PVR sorted source vertex range is not an index range");
		suite.Expect(sortedWritten&&ReadPvrScenePacket(path,7,"fixture",decoded,error)
			&&decoded.draws[1].vertexRange&&decoded.draws[1].state.first==4
			&&decoded.sortedTriangles.size()==1&&decoded.sortedTriangles[0].first==3
			&&decoded.sortedTriangles[0].count==3&&decoded.sortedTriangles[0].polyIndex==0,
			"PVR sorted source topology roundtrip");
		if(sortedWritten) {
			std::ifstream sortedFile(path,std::ios::binary);
			const auto sortedGolden=nlohmann::json::parse(sortedFile);sortedFile.close();
			bad=sortedGolden;bad["draws"][1]["range_space"]="indices";
			reject(bad,"pvr-decode-draw-range","PVR sorted source cannot masquerade as indexed range");
			bad=sortedGolden;bad["draws"][0]["range_space"]="vertices";
			reject(bad,"pvr-decode-range-space","PVR opaque indexed range cannot masquerade as source vertices");
			bad=sortedGolden;bad["sorted_triangles"][0]["poly_index"]=1u;
			reject(bad,"pvr-decode-sorted-range","PVR sorted material reference is bounded");
			bad=sortedGolden;bad["sorted_triangles"][0]["count"]=2u;
			reject(bad,"pvr-decode-sorted-range","PVR sorted commands require whole triangles");
			bad=sortedGolden;bad["sorted_triangles"][0]["first"]=4u;
			reject(bad,"pvr-decode-sorted-range","PVR sorted index range is bounded separately");
			bad=sortedGolden;bad["passes"][0]["autosort"]=false;
			reject(bad,"pvr-decode-sorted-pass","PVR sorted commands require sorted pass");
			bad=sortedGolden;bad["passes"][0]["sorted_tr"]=0u;
			reject(bad,"pvr-decode-pass-coverage","PVR sorted commands cannot escape pass coverage");
			bad=sortedGolden;bad["indices"][3]=0xffffffffu;
			reject(bad,"pvr-decode-sorted-restart","PVR triangle lists cannot contain strip restart");
			bad=sortedGolden;bad["sorted_triangles"].push_back(bad["sorted_triangles"][0]);
			reject(bad,"pvr-decode-sorted-range","PVR sorted commands cannot overlap or amplify validation work");
		}
		std::filesystem::remove(path);
		ctx.sortedTriangles[0].polyIndex=1;
		suite.Expect(!WritePvrScenePacket(path,ctx,viewport,7,"fixture",error)
			&&error=="pvr-packet-sorted-range"&&!std::filesystem::exists(path),"PVR writer rejects invalid sorted material before output");
		ctx.sortedTriangles[0]={0,0,0};
		suite.Expect(WritePvrScenePacket(path,ctx,viewport,7,"fixture",error)
			&&ReadPvrScenePacket(path,7,"fixture",decoded,error)&&decoded.sortedTriangles[0].count==0,
			"PVR empty sorted command preserves explicit source-range mode");
		std::filesystem::remove(path);
		bad=golden;bad["schema"]="flycast-pvr-scene-v1";bad.erase("sorted_triangles");
		bad["draws"][0].erase("range_space");bad["omissions"].push_back("sorted-translucency-resolve-order");
		{std::ofstream f(path,std::ios::binary);f<<bad.dump();}
		suite.Expect(ReadPvrScenePacket(path,7,"fixture",decoded,error)&&!decoded.sortedOrderCaptured,
			"PVR historical v1 remains readable without claiming captured sorted order");
		std::filesystem::remove(path);
		ctx.global_param_tr.clear();ctx.sortedTriangles.clear();ctx.verts.resize(3);ctx.idx={0,1,2};
		ctx.render_passes[0].tr_count=0;ctx.render_passes[0].sorted_tr_count=0;
		ctx.render_passes[0].autosort=false;
		ctx.verts.push_back(::Vertex{});ctx.verts.back().z=std::numeric_limits<float>::infinity();
		suite.Expect(WritePvrScenePacket(path,ctx,viewport,7,"fixture",error),"PVR unused nonfinite buffer data remains observable");
		std::ifstream nonfiniteFile(path,std::ios::binary);
		const std::string nonfiniteText((std::istreambuf_iterator<char>(nonfiniteFile)),{});nonfiniteFile.close();
		suite.Expect(nonfiniteText.find("\"nonfinite_position_count\":1")!=std::string::npos
			&&nonfiniteText.find("\"nonfinite_index_reference_count\":0")!=std::string::npos,
			"PVR referenced geometry distinguished from unused nonfinite data");
		suite.Expect(ReadPvrScenePacket(path,7,"fixture",decoded,error)&&decoded.unusedNonfinite==1
			&&std::isinf(decoded.vertices.back().z),"PVR decoder retains unused nonfinite data without promoting it to referenced geometry");
		std::filesystem::remove(path);ctx.verts.resize(3);
		ctx.idx[2]=99;
		suite.Expect(!WritePvrScenePacket(path,ctx,viewport,7,"fixture",error)
			&&error=="pvr-packet-index"&&!std::filesystem::exists(path),"PVR bad index rejected before file creation");
		ctx.idx[2]=2;ctx.global_param_op[0].count=4;
		suite.Expect(!WritePvrScenePacket(path,ctx,viewport,7,"fixture",error)
			&&error=="pvr-packet-draw-range","PVR invalid draw range rejected");
		ctx.global_param_op[0].count=3;ctx.isRTT=true;
		suite.Expect(!WritePvrScenePacket(path,ctx,viewport,7,"fixture",error),"PVR RTT export rejected");
		ctx.isRTT=false;ctx.verts.resize(65537);
		suite.Expect(!WritePvrScenePacket(path,ctx,viewport,7,"fixture",error)
			&&error=="pvr-packet-identity-or-bound","PVR vertex budget checked before output");
	}
	suite.passed += remakeCounts.passed;
	{
		const auto marker=nlohmann::json::parse(R"({"schema":1,"completed":true,"in_memory":true,"save_allowed":true,"saved":true,"loaded":true,"save_main_frame":1000,"load_main_frame":1030,"state_bytes":1024})");
		suite.Expect(ValidCaptureSaveMarker(marker,1000,30),"capture save marker valid typed completion");
		for(const char* key : {"completed","in_memory","saved","loaded","save_allowed"}) {
			auto wrong=marker;wrong[key]=false;
			suite.Expect(!ValidCaptureSaveMarker(wrong,1000,30),std::string("capture rejects false marker ")+key);
		}
		for(const auto& value : {nlohmann::json(-1),nlohmann::json(1030.5),nlohmann::json("1030"),nlohmann::json(true)}) {
			auto wrong=marker;wrong["load_main_frame"]=value;
			suite.Expect(!ValidCaptureSaveMarker(wrong,1000,30),"capture rejects non-unsigned load frame");
		}
		auto wrong=marker;wrong["load_main_frame"]=std::uint64_t(999);
		suite.Expect(!ValidCaptureSaveMarker(wrong,1000,30),"capture rejects reversed load frame");
		wrong=marker;wrong.erase("saved");
		suite.Expect(!ValidCaptureSaveMarker(wrong,1000,30),"capture rejects missing completion field");
	}
	{
		ProducerIdentityClock clock;
		ProducerIdentity missing;
		suite.Expect(!missing.Available() && !clock.Owns(missing), "producer missing identity unavailable");
		const auto zero = clock.Stamp(0);
		suite.Expect(zero.Available() && zero.cycle == 0 && zero.ordinal == 1,
			"producer cycle zero distinct from unavailable");
		const auto next = clock.Stamp(100);
		suite.Expect(next.epoch == zero.epoch && next.ordinal == 2 && clock.Owns(next),
			"producer ordinal increments independently of clock value");
		const auto same = clock.Stamp(100);
		suite.Expect(same.ordinal == 3 && same.epoch == next.epoch, "producer same cycle remains distinct submission");
		clock.Reset();
		suite.Expect(!clock.Owns(next), "producer reset rejects stale epoch");
		const auto reset = clock.Stamp(100);
		suite.Expect(reset.epoch == next.epoch+1 && reset.ordinal == 1, "producer explicit reset starts new epoch");
		const auto rollback = clock.Stamp(99);
		suite.Expect(rollback.epoch == reset.epoch+1 && rollback.ordinal == 1 && !clock.Owns(reset),
			"producer backwards clock invalidates prior epoch");
		const auto copied = rollback;
		suite.Expect(clock.Owns(copied) && copied.cycle == 99, "producer stamp copied without clock reinterpretation");
		rend_context context;
		context.captureProducer = copied;
		context.Clear();
		suite.Expect(context.captureProducer.Available(), "producer survives render-data clear for decode");
		context.InvalidateCaptureProducer();
		suite.Expect(!context.captureProducer.Available(), "producer lifecycle invalidation clears stamp");
	}
	suite.failed += remakeCounts.failed;
	{
		TaProvenance p(2); p.Begin(7);
		suite.Expect(p.Add(7, 32, 32, 1, 128), "TA origin accepts bounded interval");
		auto origin = p.Resolve(7, 36, 12);
		suite.Expect(origin && origin->sourceOffset == 132 && origin->transfer == 1, "TA origin resolves interior offset");
		suite.Expect(!p.Resolve(6, 36, 12) && !p.Resolve(7, 64, 1), "TA origin rejects stale epoch and exclusive end");
		suite.Expect(p.Add(7, 64, 32, 2, 512) && !p.Resolve(7, 48, 32), "TA split packet not silently joined");
		suite.Expect(!p.Add(7, 96, 32, 3, 0) && !p.Resolve(7, 32, 4), "TA overflow invalidates whole observation");
		p.Begin(8); p.Add(8, 0, 32, 1, 0);
		suite.Expect(!p.Add(8, 16, 32, 2, 0) && !p.Resolve(8, 0, 4), "TA overlap fails closed");
		p.Begin(9);
		suite.Expect(!p.Add(9, UINT64_MAX-15, 32, 1, 0), "TA interval addition overflow rejected");
		std::vector<std::optional<TaOrigin>> decoded{TaOrigin{7,1,128},TaOrigin{7,2,256}};
		auto mapped = RemappedOrigin(decoded, {1,0}, 0, 7);
		suite.Expect(mapped && mapped->transfer == 2, "TA explicit compaction map preserves origin");
		suite.Expect(!RemappedOrigin(decoded, {UINT32_MAX}, 0, 7) && !RemappedOrigin(decoded, {}, 0, 7), "TA missing remap rejects restart and absent vertex");
		suite.Expect(!RemappedOrigin(decoded, {1,0}, 0, 8) && !RemappedOrigin(decoded, {1,0}, 0, 0), "TA remap rejects stale or missing generation");
		p.Begin(10); p.Add(10, 0, 32, 1, 0);
		p.Begin(10);
		suite.Expect(!p.Add(10, 0, 32, 1, 0), "TA reused generation cannot revive stale identity");
		p.Begin(9);
		suite.Expect(!p.Add(9, 0, 32, 1, 0), "TA backwards generation cannot revive stale identity");
		p.Begin(10);
		suite.Expect(!p.Add(10, 0, 32, 1, 0), "TA failed reset preserves generation high water mark");
		p.Begin(11);
		suite.Expect(p.Add(11, 0, 32, 1, 0) && p.Resolve(11, 0, 32), "TA fresh generation recovers after invalidation");
	}
	std::cout << "selftest passed=" << suite.passed << " failed=" << suite.failed << '\n';
	return suite.failed == 0 ? 0 : 1;
}

} // namespace neuraltest
