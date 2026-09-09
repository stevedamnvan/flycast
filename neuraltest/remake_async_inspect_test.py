import unittest
from pathlib import Path
from unittest.mock import patch, MagicMock
import numpy as np
from remake_async_inspect import inspect


class AsyncInspectTest(unittest.TestCase):
    def setUp(self):
        self.p = "Remake async publish: frame=10 producer=9 sequence=1 bytes=30 digest=40 capture=false wait=false presentation=false\n"
        self.p += "Remake async return: source=10 producer=9 sequence=1 current=12 retained=1 presentation=false\n"
        self.c = "live_receive sequence=1 frame=10 producer=9 bytes=30 digest=40 saved_packets_read=false\n"
        self.c += "live_return sequence=1 frame=10 published=1 depth_values=307200 error= presentation_proven=false\n"
        image = MagicMock()
        image.size = (640, 480)
        self.open = patch("remake_async_inspect.Image.open")
        self.open.start().return_value.__enter__.return_value = image
        self.array = patch("remake_async_inspect.np.asarray", return_value=np.ones((480, 640, 3), dtype=np.uint8))
        self.array.start()
        self.depth = patch("remake_async_inspect.np.fromfile", return_value=np.ones(640 * 480 * 4, dtype=np.float32))
        self.depth.start()
        self.addCleanup(patch.stopall)

    def test_bounded_receipts_not_presentation(self):
        result = inspect(self.p, self.c, Path("unused"))
        self.assertEqual(result["retained_age_max"], 2)
        self.assertFalse(result["presentation_proven"])

    def test_wrong_digest_rejected(self):
        with self.assertRaisesRegex(ValueError, "receipt mismatch"):
            inspect(self.p, self.c.replace("digest=40", "digest=41"), Path("unused"))

    def test_original_overlay_receipt_required(self):
        with self.assertRaisesRegex(ValueError, "original overlays"):
            inspect(self.p, self.c, Path("unused"), True)
        overlay = "Remake async overlay retained: frame=10 sequence=1 original_native=true original_mask=true presentation=false\n"
        self.assertTrue(inspect(self.p + overlay, self.c, Path("unused"), True)["original_overlay_receipts_required"])
        with self.assertRaisesRegex(ValueError, "original overlays"):
            inspect(self.p + overlay.replace("frame=10", "frame=12"), self.c, Path("unused"), True)

    def test_duplicate_publication_rejected(self):
        with self.assertRaisesRegex(ValueError, "duplicate"):
            inspect(self.p + self.p, self.c, Path("unused"))

    def test_expired_retention_rejected(self):
        with self.assertRaisesRegex(ValueError, "identity/age"):
            inspect(self.p.replace("current=12", "current=19"), self.c, Path("unused"))

    def test_missing_return_not_silently_dropped(self):
        with self.assertRaisesRegex(ValueError, "missing paired"):
            inspect(self.p, self.c.split("live_return")[0], Path("unused"))

    def test_busy_drop_reported_not_returned(self):
        p = self.p + "Remake async publish: frame=11 producer=10 sequence=2 bytes=30 digest=41 capture=false wait=false presentation=false\n"
        c = self.c + "live_receive sequence=2 frame=11 producer=10 bytes=30 digest=41 saved_packets_read=false\n"
        c += "live_return sequence=2 frame=11 published=0 depth_values=307200 error=return-busy presentation_proven=false\n"
        result = inspect(p, c, Path("unused"))
        self.assertEqual((result["busy_return_drops"], result["retained_pairs"]), (1, 1))
