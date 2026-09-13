"""Capture import tool registered on the existing Toolkit MCP server.

Runs inside Toolkit; no second server and no generation or model downloads.
The rebased capture owns no copied material/mesh files, avoiding hash-name
collisions with captures already linked to the project.
"""
import hashlib
import json
import math
import re
import struct
from pathlib import Path


def scalar_dds_mip_probe(source_file, semantic, project_file=None, ingested_file=None):
    """Read-only authored-chain check; does not ingest, create metadata or bind.

    BC4_UNORM has no sRGB variant. Material linear sampling remains a separate
    typed-binding check; byte preservation alone does not establish that state.
    """
    if semantic not in ('ROUGHNESS', 'METALLIC'):
        raise ValueError('Scalar semantic required')

    def inspect(path):
        path = Path(path)
        if not path.is_absolute() or path.suffix.lower() != '.dds' or not path.is_file():
            raise ValueError('Existing absolute DDS required')
        if not 148 <= path.stat().st_size <= 64 * 1024 * 1024:
            raise ValueError('DDS extent bound')
        data = path.read_bytes()
        if data[:4] != b'DDS ' or struct.unpack_from('<I', data, 4)[0] != 124:
            raise ValueError('DDS header required')
        height, width = struct.unpack_from('<II', data, 12)
        flags = struct.unpack_from('<I', data, 8)[0]
        caps = struct.unpack_from('<I', data, 108)[0]
        if flags & 0x21007 != 0x21007 or caps & 0x401008 != 0x401008 or struct.unpack_from('<I', data, 80)[0] != 4:
            raise ValueError('Declared texture, mip chain and FourCC flags required')
        mip_count = struct.unpack_from('<I', data, 28)[0]
        if not width or not height or width > 16384 or height > 16384:
            raise ValueError('DDS dimensions bound')
        if mip_count != max(width, height).bit_length():
            raise ValueError('Complete mip chain required')
        if struct.unpack_from('<I', data, 76)[0] != 32 or data[84:88] != b'DX10':
            raise ValueError('Explicit DX10 BC4_UNORM required')
        dxgi, dimension, misc, array_size, misc2 = struct.unpack_from('<5I', data, 128)
        if (dxgi, dimension, misc, array_size, misc2) != (80, 3, 0, 1, 0):
            raise ValueError('Single 2D BC4_UNORM linear scalar required')
        if struct.unpack_from('<I', data, 112)[0] != 0 or struct.unpack_from('<I', data, 24)[0] not in (0, 1):
            raise ValueError('No cube or volume DDS')
        offset = 148
        mips = []
        for level in range(mip_count):
            w, h = max(1, width >> level), max(1, height >> level)
            size = ((w + 3) // 4) * ((h + 3) // 4) * 8
            if offset + size > len(data):
                raise ValueError('Truncated mip payload')
            mips.append(dict(level=level, width=w, height=h, bytes=size,
                             sha256=hashlib.sha256(data[offset:offset + size]).hexdigest()))
            offset += size
        if offset != len(data):
            raise ValueError('Trailing DDS payload')
        return dict(path=str(path), sha256=hashlib.sha256(data).hexdigest(),
                    format='BC4_UNORM', mips=mips)

    source = inspect(source_file)
    result = dict(source=source, semantic=semantic, read_only=True,
                  pipeline_preservation_proven=False, linear_binding_verified=False)
    if ingested_file is not None:
        if project_file is None:
            raise ValueError('Saved project required for output check')
        project, target = Path(project_file), Path(ingested_file)
        if not project.is_absolute() or not project.is_file():
            raise ValueError('Saved absolute project required')
        allowed = (project.parent / 'assets/ingested').resolve()
        if allowed not in target.resolve().parents or target.resolve() == Path(source_file).resolve():
            raise ValueError('Separate ingested project output required')
        suffix = '.m.rtex.dds' if semantic == 'METALLIC' else '.r.rtex.dds'
        if not target.name.endswith(suffix) or not Path(str(target) + '.meta').is_file():
            raise ValueError('Typed ingested DDS and existing metadata required')
        output = inspect(target)
        result.update(output=output, all_mips_byte_identical=source['mips'] == output['mips'])
    return result


def displacement_request(shader_path, displace_in=None, displace_out=None):
    if not re.fullmatch(r'/RootNode/Looks/mat_[0-9A-Fa-f]{16}/Shader', shader_path):
        raise ValueError('Expected one captured material shader path')
    values = {'displace_in': displace_in, 'displace_out': displace_out}
    for value in values.values():
        if value is not None and (isinstance(value, bool) or not isinstance(value, (float, int))
                                  or not math.isfinite(value) or not 0 <= value <= 0.2):
            raise ValueError('Displacement ranges must be finite numbers in 0..0.2')
    return {name: float(value) for name, value in values.items() if value is not None}


def surface_request(shader_path, roughness, metallic):
    displacement_request(shader_path)
    values = {'reflection_roughness_constant': roughness, 'metallic_constant': metallic}
    for value in values.values():
        if isinstance(value, bool) or not isinstance(value, (float, int)) or not math.isfinite(value) or not 0 <= value <= 1:
            raise ValueError('Surface constants must be finite numbers in 0..1')
    return {name: float(value) for name, value in values.items()}


def diffuse_binding_request(project_file, layer_file, shader_path, texture_file):
    displacement_request(shader_path)
    project, layer, texture = map(Path, (project_file, layer_file, texture_file))
    if not all(p.is_absolute() and p.is_file() for p in (project, layer, texture)):
        raise ValueError('Existing absolute project, layer and texture required')
    root = project.resolve().parent
    if layer.resolve().parent != root / 'layers' or layer.name in {
            'pbrify_cloth_refined_v2.usda', 'pbrify_reimagined.usda', 'curated_pbr.usda', 'ai_pbr_draft.usda'}:
        raise ValueError('Separate opt-in layer required')
    if texture.suffix.lower() != '.dds' or root / 'assets/ingested' not in texture.resolve().parents:
        raise ValueError('Project ingested DDS required')
    if not Path(str(texture) + '.meta').is_file():
        raise ValueError('Ingestion metadata required')
    return layer.resolve(), texture.resolve()


def capture_destination(project_file, capture_file):
    project, source = Path(project_file), Path(capture_file)
    if not project.is_absolute() or not source.is_absolute():
        raise ValueError('Absolute project and capture paths required')
    if source.suffix.lower() not in ('.usd', '.usda', '.usdc'):
        raise ValueError('USD capture required')
    if not source.is_file() or not project.is_file():
        raise ValueError('Project and capture must exist')
    capture_dir = (project.parent / 'deps' / 'captures').resolve()
    if not capture_dir.is_dir():
        raise ValueError('Existing linked project capture directory required')
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    target = capture_dir / ('capture_import_' + digest[:16] + '.usda')
    if target.exists():
        raise ValueError('Destination exists; inspect/reuse it without importing again')
    return source, target, digest


def ingestion_request(project_file, request_json, semantic='DIFFUSE'):
    if semantic not in ('DIFFUSE', 'ROUGHNESS', 'METALLIC'):
        raise ValueError('Unsupported ingestion semantic')
    project = Path(project_file)
    if not project.is_absolute() or not project.is_file():
        raise ValueError('Saved local project required')
    body = json.loads(request_json)
    data = body['context_plugin']['data']
    files = data['input_files']
    if len(files) != 1 or len(files[0]) != 2 or files[0][1] != semantic:
        raise ValueError('Exactly one ' + semantic + ' PNG required')
    source = Path(files[0][0])
    if not source.is_absolute() or source.suffix.lower() != '.png' or not source.is_file():
        raise ValueError('Existing absolute PNG required')
    output = Path(data['output_directory']).resolve()
    allowed = (project.parent / 'assets/ingested').resolve()
    if output == allowed or allowed not in output.parents:
        raise ValueError('Separate project assets/ingested subdirectory required')
    if output.exists() and (not output.is_dir() or any(output.iterdir())):
        raise ValueError('Output must be empty; inspect cached results before reuse')
    body['executor'] = 0
    return body


def scalar_dds_ingestion_request(project_file, request_json, semantic):
    project = Path(project_file)
    if not project.is_absolute() or not project.is_file():
        raise ValueError('Saved absolute project required')
    body = json.loads(request_json)
    if body['context_plugin']['name'] != 'TextureImporter':
        raise ValueError('Existing TextureImporter required')
    data = body['context_plugin']['data']
    files = data['input_files']
    if len(files) != 1 or len(files[0]) != 2 or files[0][1] != semantic:
        raise ValueError('Exactly one scalar DDS semantic required')
    probe = scalar_dds_mip_probe(files[0][0], semantic)
    source = Path(files[0][0]).resolve()
    output = Path(data['output_directory'])
    if not output.is_absolute():
        raise ValueError('Absolute output required')
    output = output.resolve()
    allowed = (project.parent / 'assets/ingested').resolve()
    if output == allowed or allowed not in output.parents or source == output or output in source.parents:
        raise ValueError('Separate isolated project output required')
    if output.exists() and (not output.is_dir() or any(output.iterdir())):
        raise ValueError('Output must be fresh and empty')
    def preserve(value):
        if isinstance(value, dict):
            for key, child in value.items():
                if key == 'cleanup_input' and child is not False:
                    raise ValueError('Input cleanup forbidden')
                preserve(child)
        elif isinstance(value, list):
            for child in value:
                preserve(child)
    preserve(body)
    body['executor'] = 0
    return body, probe


def register(mcp):
    @mcp.tool(name='flycast_ingest_scalar_dds_current_process')
    async def ingest_scalar_dds_current_process(request_json: str, semantic: str) -> dict:
        """Test existing scalar DDS ingestion; verify authored mips, never bind or fabricate metadata."""
        import omni.usd
        import httpx
        from omni.services.core import main
        stage = omni.usd.get_context().get_stage()
        if stage is None:
            raise ValueError('Open intended project first')
        project = stage.GetRootLayer().realPath
        body, before = scalar_dds_ingestion_request(project, request_json, semantic)
        async with httpx.AsyncClient(transport=httpx.ASGITransport(app=main.get_app()),
                                     base_url='http://fastapi', timeout=120) as client:
            response = await client.post('/ingestcraft/mass-validator/queue/material', json=body)
            response.raise_for_status()
            service_result = response.json()
        source = before['source']['path']
        if scalar_dds_mip_probe(source, semantic)['source']['sha256'] != before['source']['sha256']:
            raise ValueError('Ingestion changed source DDS')
        output = Path(body['context_plugin']['data']['output_directory']).resolve()
        checks = []
        candidates = list(output.rglob('*.dds')) if output.is_dir() else []
        if len(candidates) > 32:
            raise ValueError('Unexpected DDS output count')
        for candidate in candidates:
            if output not in candidate.resolve().parents:
                raise ValueError('Output escaped isolated directory')
            try:
                check = scalar_dds_mip_probe(source, semantic, project, str(candidate))
            except ValueError as exc:
                check = dict(path=str(candidate), all_mips_byte_identical=False, error=str(exc))
            checks.append(check)
        preserved = len(checks) == 1 and checks[0].get('all_mips_byte_identical', False)
        return dict(result=service_result, executor=0, bound=False, source=before['source'],
                    checks=checks, preservation_verified=preserved,
                    disposition='preserved_fixture_only' if preserved else 'rejected_unproven_mip_preservation',
                    linear_binding_verified=False)

    @mcp.tool(name='flycast_probe_scalar_dds_mips')
    async def probe_scalar_dds_mips(source_file: str, semantic: str, project_file: str = None, ingested_file: str = None) -> dict:
        """Read-only scalar DDS mip validation; never ingests, binds or creates metadata."""
        return scalar_dds_mip_probe(source_file, semantic, project_file, ingested_file)

    async def ingest_current_process(request_json, semantic):
        """Ingest one diffuse through Toolkit's existing API, avoiding broken enum refs.

        Uses supported current-process executor. No layer binding or model work.
        Requires a fresh project ingestion subdirectory; never overwrites cache.
        """
        import omni.usd
        import httpx
        from omni.services.core import main
        stage = omni.usd.get_context().get_stage()
        if stage is None:
            raise ValueError('Open the intended project first')
        body = ingestion_request(stage.GetRootLayer().realPath, request_json, semantic)
        async with httpx.AsyncClient(transport=httpx.ASGITransport(app=main.get_app()),
                                     base_url='http://fastapi', timeout=120) as client:
            response = await client.post('/ingestcraft/mass-validator/queue/material', json=body)
            response.raise_for_status()
            return dict(result=response.json(), executor=0, bound=False)

    @mcp.tool(name='flycast_ingest_diffuse_current_process')
    async def ingest_diffuse_current_process(request_json: str) -> dict:
        """Ingest one DIFFUSE PNG into a fresh project directory; no binding."""
        return await ingest_current_process(request_json, 'DIFFUSE')

    @mcp.tool(name='flycast_ingest_roughness_current_process')
    async def ingest_roughness_current_process(request_json: str) -> dict:
        """Ingest one ROUGHNESS PNG using Toolkit semantic conversion; no binding.

        Requires a fresh project ingestion directory and never overwrites cache.
        """
        return await ingest_current_process(request_json, 'ROUGHNESS')

    @mcp.tool(name='flycast_ingest_metallic_current_process')
    async def ingest_metallic_current_process(request_json: str) -> dict:
        """Ingest one METALLIC PNG through existing semantic conversion; no binding.

        Requires a fresh project ingestion directory and never overwrites cache.
        """
        return await ingest_current_process(request_json, 'METALLIC')

    @mcp.tool(name='flycast_inspect_displacement')
    async def inspect_displacement(shader_path: str) -> dict:
        """Read composed USD displacement values and their authored layer sources.

        An absent value is null, not an inferred MDL/runtime default.
        """
        import omni.usd
        from pxr import Usd
        displacement_request(shader_path)
        stage = omni.usd.get_context().get_stage()
        if stage is None:
            raise ValueError('Open the intended project first')
        prim = stage.GetPrimAtPath(shader_path)
        if not prim or prim.GetTypeName() != 'Shader':
            raise ValueError('Existing Shader required')
        result = {}
        for name in ('displace_in', 'displace_out', 'height_texture'):
            attr = prim.GetAttribute('inputs:' + name)
            value = attr.Get() if attr else None
            result[name] = dict(value=str(value) if name == 'height_texture' and value is not None else value,
                                authored=bool(attr and attr.HasAuthoredValueOpinion()),
                                layers=[s.layer.identifier for s in attr.GetPropertyStack(Usd.TimeCode.Default())] if attr else [])
        return dict(shader=shader_path, inputs=result, runtime_defaults_verified=False)

    @mcp.tool(name='flycast_set_displacement')
    async def set_displacement(shader_path: str, displace_in: float, displace_out: float) -> dict:
        """Author bounded displacement ranges in an existing project layers/ edit target.

        Does not save or alter textures. Requires a non-baseline opt-in layer;
        rejects original baseline layers and restores layer content on failure.
        """
        import omni.usd
        from pxr import Sdf
        values = displacement_request(shader_path, displace_in, displace_out)
        stage = omni.usd.get_context().get_stage()
        if stage is None:
            raise ValueError('Open the intended project first')
        prim = stage.GetPrimAtPath(shader_path)
        if not prim or prim.GetTypeName() != 'Shader':
            raise ValueError('Existing Shader required')
        root, layer = stage.GetRootLayer(), stage.GetEditTarget().GetLayer()
        if not root.realPath or not layer.realPath:
            raise ValueError('Saved project and edit layer required')
        allowed = (Path(root.realPath).parent / 'layers').resolve()
        target = Path(layer.realPath).resolve()
        if target.parent != allowed or target.name in {
                'pbrify_cloth_refined_v2.usda', 'pbrify_reimagined.usda', 'curated_pbr.usda', 'ai_pbr_draft.usda'}:
            raise ValueError('Select a separate opt-in project layers/ layer')
        for name in values:
            attr = prim.GetAttribute('inputs:' + name)
            if attr and attr.GetTypeName() != Sdf.ValueTypeNames.Float:
                raise ValueError('Existing displacement input must be Float')
        snapshot = layer.ExportToString()
        try:
            for name, value in values.items():
                attr = prim.CreateAttribute('inputs:' + name, Sdf.ValueTypeNames.Float, custom=False)
                if not attr.Set(value):
                    raise RuntimeError('Displacement authoring failed')
        except Exception:
            layer.ImportFromString(snapshot)
            raise
        return dict(shader=shader_path, authored=values, layer=layer.identifier, saved=False)

    @mcp.tool(name='flycast_set_surface_constants')
    async def set_surface_constants(shader_path: str, roughness: float, metallic: float) -> dict:
        """Author bounded roughness and metallic constants in an existing project layers/ edit target.

        Does not save or alter textures. Requires a non-baseline opt-in layer;
        rejects original baseline layers and restores layer content on failure.
        """
        import omni.usd
        from pxr import Sdf
        values = surface_request(shader_path, roughness, metallic)
        stage = omni.usd.get_context().get_stage()
        if stage is None:
            raise ValueError('Open the intended project first')
        prim = stage.GetPrimAtPath(shader_path)
        if not prim or prim.GetTypeName() != 'Shader':
            raise ValueError('Existing Shader required')
        root, layer = stage.GetRootLayer(), stage.GetEditTarget().GetLayer()
        if not root.realPath or not layer.realPath:
            raise ValueError('Saved project and edit layer required')
        allowed = (Path(root.realPath).parent / 'layers').resolve()
        target = Path(layer.realPath).resolve()
        if target.parent != allowed or target.name in {
                'pbrify_cloth_refined_v2.usda', 'pbrify_reimagined.usda', 'curated_pbr.usda', 'ai_pbr_draft.usda'}:
            raise ValueError('Select a separate opt-in project layers/ layer')
        for name in values:
            attr = prim.GetAttribute('inputs:' + name)
            if attr and attr.GetTypeName() != Sdf.ValueTypeNames.Float:
                raise ValueError('Existing surface input must be Float')
        material = prim.GetParent()
        if material.GetTypeName() != 'Material':
            raise ValueError('Existing parent Material required')
        # Flatten resolves referenced assets; copy only this material, never scene geometry.
        composed = stage.Flatten()
        material_path = material.GetPath()
        snapshot = layer.ExportToString()
        try:
            Sdf.CreatePrimInLayer(layer, material_path.GetParentPath())
            if not Sdf.CopySpec(composed, material_path, layer, material_path):
                raise RuntimeError('Material definition copy failed')
            for name, value in values.items():
                attr = prim.CreateAttribute('inputs:' + name, Sdf.ValueTypeNames.Float, custom=False)
                if not attr.Set(value):
                    raise RuntimeError('Surface authoring failed')
        except Exception:
            layer.ImportFromString(snapshot)
            raise
        return dict(shader=shader_path, authored=values, layer=layer.identifier, saved=False)

    @mcp.tool(name='flycast_bind_diffuse_in_layer')
    async def bind_diffuse_in_layer(layer_file: str, shader_path: str, texture_file: str) -> dict:
        """Bind one ingested diffuse explicitly in a candidate layer, without saving.

        Never relies on the UI edit target; rolls back target content on failure.
        Rejects baseline and unlinked layers. Other stage layers remain untouched.
        """
        import omni.usd
        from pxr import Sdf, Usd
        stage = omni.usd.get_context().get_stage()
        if stage is None:
            raise ValueError('Open the intended project first')
        target, texture = diffuse_binding_request(stage.GetRootLayer().realPath, layer_file, shader_path, texture_file)
        layer = next((x for x in stage.GetLayerStack() if x.realPath and Path(x.realPath).resolve() == target), None)
        if layer is None:
            raise ValueError('Candidate layer must be in current stage')
        prim = stage.GetPrimAtPath(shader_path)
        if not prim or prim.GetTypeName() != 'Shader' or prim.GetParent().GetTypeName() != 'Material':
            raise ValueError('Existing captured Material and Shader required')
        baseline = Sdf.Layer.FindOrOpen(str(Path(stage.GetRootLayer().realPath).parent / 'mod.usda'))
        if baseline is None:
            raise ValueError('Existing baseline required')
        baseline_text = baseline.ExportToString()
        baseline_bytes = Path(baseline.realPath).read_bytes()
        snapshot = layer.ExportToString()
        composed = stage.Flatten()
        path = prim.GetParent().GetPath()
        try:
            with Usd.EditContext(stage, layer):
                Sdf.CreatePrimInLayer(layer, path.GetParentPath())
                if not Sdf.CopySpec(composed, path, layer, path):
                    raise RuntimeError('Material copy failed')
                attr = stage.GetPrimAtPath(shader_path).CreateAttribute('inputs:diffuse_texture', Sdf.ValueTypeNames.Asset, custom=False)
                if not attr.Set(Sdf.AssetPath(str(texture))):
                    raise RuntimeError('Diffuse write failed')
            spec = layer.GetAttributeAtPath(shader_path + '.inputs:diffuse_texture')
            if not spec or spec.default != Sdf.AssetPath(str(texture)):
                raise RuntimeError('Explicit layer verification failed')
            if baseline.ExportToString() != baseline_text or Path(baseline.realPath).read_bytes() != baseline_bytes:
                raise RuntimeError('Baseline changed during candidate binding')
        except Exception:
            layer.ImportFromString(snapshot)
            raise
        return dict(layer=str(target), shader=shader_path, texture=str(texture), saved=False, baseline_unchanged=True)

    @mcp.tool(name='flycast_activate_capture')
    async def activate_capture(capture_file: str) -> dict:
        """Select an existing capture in this project's linked capture directory.

        No file copying, rewriting or saving; uses Toolkit's capture API.
        """
        import omni.usd
        from lightspeed.trex.capture.core.shared import Setup
        context = omni.usd.get_context()
        stage = context.get_stage()
        if stage is None:
            raise ValueError('Open the intended project first')
        root = stage.GetRootLayer()
        allowed = (Path(root.realPath).parent / 'deps/captures').resolve()
        capture = Path(capture_file).resolve()
        if capture.parent != allowed or not capture.is_file():
            raise ValueError('Capture must exist in the linked capture directory')
        if not Setup.is_capture_file(str(capture)):
            raise ValueError('Not a Toolkit capture')
        Setup('').import_capture_layer(str(capture), do_undo=True)
        return dict(capture=str(capture), project_saved=False)

    @mcp.tool(name='flycast_import_capture')
    async def import_capture(capture_file: str, dry_run: bool = True) -> dict:
        """Import one capture via Toolkit's capture API, with a collision-safe dry run.

        Requires an open saved project. Keeps external dependencies at their
        original absolute paths. Does not save the project or change mod.usda.
        """
        import omni.usd
        from pxr import Sdf, UsdUtils
        from lightspeed.trex.capture.core.shared import Setup

        context = omni.usd.get_context()
        stage = context.get_stage()
        if stage is None:
            raise ValueError('Open the intended project first')
        root = stage.GetRootLayer()
        source, target, digest = capture_destination(root.realPath, capture_file)
        original = Sdf.Layer.FindOrOpen(str(source))
        if not original or not Setup.is_layer_a_capture_file(original):
            raise ValueError('Source is not a Toolkit capture')
        candidate = Sdf.Layer.CreateAnonymous('capture-import.usda')
        candidate.TransferContent(original)
        dependencies = []

        def rebase(path):
            if not path:
                return path
            absolute = Sdf.ComputeAssetPathRelativeToLayer(original, path)
            dependencies.append(absolute)
            return absolute

        UsdUtils.ModifyAssetPaths(candidate, rebase)
        missing = [p for p in dependencies if not Path(p).is_file()]
        if missing:
            raise ValueError('Missing/non-local capture dependencies: ' + repr(missing[:8]))
        result = dict(project=root.realPath, source=str(source), source_sha256=digest,
                      destination=str(target), dependencies=len(dependencies),
                      dry_run=dry_run, project_saved=False, baseline_mod_changed=False)
        if dry_run:
            return result
        # Snapshot only the in-memory workfile for rollback. Never save originals.
        snapshot = Sdf.Layer.CreateAnonymous('capture-import-rollback.usda')
        snapshot.TransferContent(root)
        if not candidate.Export(str(target)):
            raise RuntimeError('Could not export rebased capture')
        try:
            Setup('').import_capture_layer(str(target), do_undo=True)
            imported = [p for p in root.subLayerPaths if Path(p).name == target.name]
            if len(imported) != 1:
                raise RuntimeError('Capture not present exactly once in workfile')
        except Exception:
            root.TransferContent(snapshot)
            raise
        result['imported'] = True
        return result
