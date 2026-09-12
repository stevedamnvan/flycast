"""Capture import tool registered on the existing Toolkit MCP server.

Runs inside Toolkit; no second server and no generation or model downloads.
The rebased capture owns no copied material/mesh files, avoiding hash-name
collisions with captures already linked to the project.
"""
import hashlib
from pathlib import Path


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


def register(mcp):
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
