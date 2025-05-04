from dataclasses import dataclass
import math
import os
import sys

@dataclass
class TimeLevelPoint:
    time_point_from: float = 0.0
    time_point_to: float = 0.0
    level_value: float = 0.0

def main() -> None:
    listOfPoints: list[TimeLevelPoint] = []
    listOfValues: list[float] = []
    time_point: float = 0.0
    level_value: float = 0.0
    with open(os.path.join("..", "_internal_levels.log")) as fin:
        for line in fin:
            if "pts_time:" in line:
                parts: list[str] = line.split()
                time_point = float(parts[2].split(":")[1])
                if len(listOfPoints) > 0:
                    listOfPoints[-1].time_point_to = time_point
            elif "lavfi.astats.Overall.Peak_level" in line:
                parts: list[str] = line.split("=")
                level_value = float(parts[-1])
                if math.isinf(level_value) and level_value < 0:
                    # negative infinity, presumably no bass, so hardcode -120.0 for now
                    level_value = -120.0
                listOfValues.append(level_value)
                listOfPoints.append(TimeLevelPoint(time_point, -1, level_value))

    minVal: float = min(listOfValues)
    maxVal: float = max(listOfValues)
    #print("minVal:", minVal)
    #print("maxVal:", maxVal)

    with open(os.path.join("..", "_internal_ball_scale.log"), "w") as fout:
        for point in listOfPoints:
            relative: float = (point.level_value - minVal) / (maxVal - minVal)
            #print("relative:", relative)
            zoom: float = 0.5 + (relative * 0.5)

            tpf_str: str = f"{point.time_point_from:.3f}"
            tpt_str: str = f"{point.time_point_to:.3f}"
            zoom_str: str = f"{zoom:.3f}"
            #tpf_str: str = f"{point.time_point_from}"
            #tpt_str: str = f"{point.time_point_to}"
            #zoom_str: str = f"{zoom}"

            if point.time_point_to >= 0:
                fout.write(f"{tpf_str} scale@circle width '{zoom_str}*in_w', scale@circle height '{zoom_str}*in_h';\n")
            else:
                fout.write(f"{tpf_str} scale@circle width '{zoom_str}*in_w', scale@circle height '{zoom_str}*in_h';\n")

if __name__ == "__main__":
    main()
