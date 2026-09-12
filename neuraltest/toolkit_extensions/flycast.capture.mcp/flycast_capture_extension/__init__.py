import importlib.util
from pathlib import Path
import omni.ext


class CaptureExtension(omni.ext.IExt):
    def on_startup(self, ext_id):
        from lightspeed.trex.mcp.core.extension import get_mcp_instance
        source = Path(__file__).resolve().parents[3] / 'remix_capture_mcp.py'
        spec = importlib.util.spec_from_file_location('flycast_capture_tools', source)
        self.module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.module)
        self.module.register(get_mcp_instance())

    def on_shutdown(self):
        self.module = None
