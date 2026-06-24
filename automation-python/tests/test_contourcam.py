"""Python-side unit tests for the ContourCAM ctypes wrapper.

Requires the built native core; point CONTOURCAM_LIB_DIR at the build output
directory (e.g. build/bin or build/bin/Release). Skipped otherwise.
"""
from __future__ import annotations

import os
from pathlib import Path

import pytest

import contourcam as cc

LIB_DIR = os.environ.get("CONTOURCAM_LIB_DIR")
SAMPLE = Path(__file__).resolve().parents[2] / "samples" / "plate_pocket_holes.dxf"


@pytest.fixture()
def core():
    if not LIB_DIR:
        pytest.skip("set CONTOURCAM_LIB_DIR to the built core directory")
    return cc.load(LIB_DIR)


def test_query_sample_geometry(core):
    with core.load_dxf(str(SAMPLE)) as doc:
        assert doc.outer_count() == 1
        circles = doc.circles()
        assert len(circles) == 4
        assert all(abs(c.radius - 3.0) < 1e-6 for c in circles)


def test_generate_and_export_is_deterministic(core, tmp_path):
    with core.load_dxf(str(SAMPLE)) as doc:
        tool = cc.ToolParams(6.0, 2, cc.END_MILL)
        job = cc.JobParams(6.0, 2.0, 0.45, 600.0, 200.0, 10000.0, 5.0, cc.CLIMB)
        with doc.generate_toolpath(tool, job) as tp:
            assert tp.segment_count() > 1
            a = tmp_path / "a.gcode"
            b = tmp_path / "b.gcode"
            tp.export_gcode(str(a))
            tp.export_gcode(str(b))
            text = a.read_text()
            assert text == b.read_text()  # determinism (NFR-6)
            assert "G21 G90 G17 G54" in text
            assert "M30" in text


def test_buffer_query_matches_count(core):
    with core.load_dxf(str(SAMPLE)) as doc:
        assert len(doc.circles()) == doc.circle_count()
