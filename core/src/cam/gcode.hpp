// gcode.hpp -- ISO 6983 / RS-274 G-code post-processor.
//
// Serializes a Toolpath into a widely compatible G-code subset (LinuxCNC / GRBL
// style, loadable in CAMotics). Output is DETERMINISTIC: identical inputs yield
// byte-identical text, which is what makes the C#<->Python parity test (M4)
// meaningful.
#ifndef CONTOURCAM_CAM_GCODE_HPP
#define CONTOURCAM_CAM_GCODE_HPP

#include <cstdint>
#include <string>

#include "cam/toolpath.hpp"

namespace contourcam {

struct PostParams {
    bool metric = true;       // true -> G21 (mm), false -> G20 (inch)
    bool coolant = false;     // emit M8 / M9
    int32_t tool_number = 1;  // T word for the tool change
};

std::string writeGcode(const Toolpath& tp, const ToolParams& tool, const JobParams& job,
                       const PostParams& post);

}  // namespace contourcam

#endif  // CONTOURCAM_CAM_GCODE_HPP
