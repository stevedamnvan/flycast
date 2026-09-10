# SPDX-License-Identifier: GPL-2.0-or-later
"""Source-matched four-lane review; not external provenance or a quality verdict."""
import argparse
import json
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw
from remake_temporal_compare import captures, rgba, digest, require


def verify_native_identity(native,combined,frame):
    require(native['frame_id']==combined['frame_id']==frame,'native frame mismatch')
    require(native['producer_identity'].get('available') is True,'native producer unavailable')
    require(all(native['producer_identity'][key]==combined['producer_identity'][key]
                for key in ('epoch','ordinal','cycle')),'native producer mismatch')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('native-dlaa','remix-only','combined','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    a=parser.parse_args()
    require(a.out.is_absolute() and not a.out.exists(),'new absolute output required')
    remix,combined=captures(a.remix_only),captures(a.combined)
    frames=sorted(combined)
    require(2<=len(frames)<=300,'bounded moving sequence required')
    panels=[]
    for frame in frames:
        native=a.native_dlaa/('frame-%06d'%frame)
        require(frame in remix,'missing Remix-only source')
        r,c=remix[frame][0],combined[frame][0]
        nm=json.loads((native/'manifest.json').read_text())
        cm=json.loads((c/'manifest.json').read_text())
        verify_native_identity(nm,cm,frame)
        original=rgba(native/'native-pvr-color.png')
        require(np.array_equal(original,rgba(c/'original-native.png')),'native pixel mismatch')
        require(remix[frame][1].get('neural_evaluation_skipped') is True,'not Remix-only')
        for name in ('remake-view.bin','returned-remix.png','remake-return-depth.f32',
                     'native-effect-identity.bin','native-alpha-exclusions.bin','original-overlay-mask.png'):
            require(digest(r/name)==digest(c/name),(frame,'input mismatch',name))
        pictures=(original,rgba(native/'final-composited.png'),rgba(r/'composited-remix.png'),rgba(c/'composited-remix.png'))
        panel=Image.new('RGB',(2560,504),'#181818');draw=ImageDraw.Draw(panel)
        for i,(label,picture) in enumerate(zip(('Native PVR','Native PVR + public DLAA Auto',
                    'Remix only + native effects/HUD','Combined experimental + native effects/HUD'),pictures)):
            draw.text((i*640+8,5),f'{label} | source {frame}',fill='white')
            panel.paste(Image.fromarray(picture),(i*640,24))
        panels.append(panel)
    report=dict(frames=frames,gaps=[(x,y) for x,y in zip(frames,frames[1:]) if y!=x+1],
        native_pixel_mismatches=0,scope='exact game producer/native pixels and frozen Remix inputs; presentation checks are separate',
        external_provenance_verified=False,performance_eligible=False,winner=None)
    a.out.mkdir()
    (a.out/'review.json').write_text(json.dumps(report,indent=2))
    panels[0].save(a.out/'moving-comparison-slow.gif',save_all=True,append_images=panels[1:],
        duration=[80*(y-x) for x,y in zip(frames,frames[1:])]+[80],loop=0,optimize=False)
    panels[len(panels)//2].save(a.out/'midpoint.png')
    print(json.dumps(report))


if __name__=='__main__':
    main()
