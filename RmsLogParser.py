from dataclasses import dataclass
import math
import os
import random
import sys

@dataclass
class TimeLevelPoint:
    time_point_from: float = 0.0
    level_value: float = 0.0

def normalToBallZoom(normal: float) -> float:
    normal = max(0.0, min(1.0, normal))
    return 0.5 + (normal * 0.5)

def normalToDisplaceRandom(normal: float) -> float:
    normal = max(0.0, min(1.0, normal))
    random_value: float = random.uniform(-0.5, 0.5)
    make_it_converge: float = math.pow(normal, 2)  # values towards 0 are more likely, instead of it being linear
    scale: float = 0.25
    return 0.5 + (random_value * make_it_converge * scale)

def getNormal(value: float, min_val: float, max_val: float) -> float:
    return (value - min_val) / (max_val - min_val)

def main() -> None:
    dictOfListOfPoints: dict[str, list[TimeLevelPoint]] = {}
    dictOfListOfValues: dict[str, list[float]] = {}

    time_point: float = 0.0
    level_value: float = 0.0
    number_time_points: int = 0
    with open(os.path.join("..", "_internal_levels.log")) as fin:
        for line in fin:
            if "pts_time:" in line:
                parts: list[str] = line.split()
                time_point = float(parts[2].split(":")[1])
                number_time_points += 1
            else:
                parts: list[str] = line.split("=")

                lineName: str = parts[0]
                if lineName not in dictOfListOfPoints:
                    dictOfListOfPoints[lineName] = []
                if lineName not in dictOfListOfValues:
                    dictOfListOfValues[lineName] = []

                level_value: float = float(parts[-1])
                dictOfListOfPoints[lineName].append(TimeLevelPoint(time_point, level_value))
                dictOfListOfValues[lineName].append(level_value)

    minVals: dict[str, float] = {}
    maxVals: dict[str, float] = {}
    for x in dictOfListOfValues:
        minVals[x] = min(dictOfListOfValues[x])
        maxVals[x] = max(dictOfListOfValues[x])
        #print(f"minVal for {x}: {minVal}")
        #print(f"maxVal for {x}: {maxVal}")

    with open(os.path.join("..", "_internal_sendcmd_cmds.log"), "w") as fout, open(os.path.join("..", "_internal_sendcmd_cmds_b.log"), "w") as fo_b, open(os.path.join("..", "_internal_sendcmd_cmds_dx.log"), "w") as fo_dx, open(os.path.join("..", "_internal_sendcmd_cmds_dy.log"), "w") as fo_dy:
        for i in range(number_time_points):
            point_min_level: TimeLevelPoint = dictOfListOfPoints["lavfi.astats.Overall.Min_level"][i]
            point_max_level: TimeLevelPoint = dictOfListOfPoints["lavfi.astats.Overall.Max_level"][i]
            point_peak_level: TimeLevelPoint = dictOfListOfPoints["lavfi.astats.Overall.Peak_level"][i]
            point_rms_level: TimeLevelPoint = dictOfListOfPoints["lavfi.astats.Overall.RMS_level"][i]

            relative_min_level: float = getNormal(point_min_level.level_value, minVals["lavfi.astats.Overall.Min_level"], maxVals["lavfi.astats.Overall.Min_level"])
            relative_max_level: float = getNormal(point_max_level.level_value, minVals["lavfi.astats.Overall.Max_level"], maxVals["lavfi.astats.Overall.Max_level"])
            relative_peak_level: float = getNormal(point_peak_level.level_value, minVals["lavfi.astats.Overall.Peak_level"], maxVals["lavfi.astats.Overall.Peak_level"])
            relative_rms_level: float = getNormal(point_rms_level.level_value, minVals["lavfi.astats.Overall.RMS_level"], maxVals["lavfi.astats.Overall.RMS_level"])

            ball_zoom: float = normalToBallZoom(relative_peak_level)
            displace_value_x_r: float = normalToDisplaceRandom(relative_max_level)
            displace_value_x_g: float = normalToDisplaceRandom(relative_max_level)
            displace_value_x_b: float = normalToDisplaceRandom(relative_max_level)
            displace_value_y_r: float = normalToDisplaceRandom(relative_max_level)
            displace_value_y_g: float = normalToDisplaceRandom(relative_max_level)
            displace_value_y_b: float = normalToDisplaceRandom(relative_max_level)

            #tpf_str: str = f"{point_max_level.time_point_from:.3f}"
            ball_zoom_str: str = f"{ball_zoom:.3f}"
            displace_value_x_r_str: str = f"{displace_value_x_r:.3f}"
            displace_value_x_g_str: str = f"{displace_value_x_g:.3f}"
            displace_value_x_b_str: str = f"{displace_value_x_b:.3f}"
            displace_value_y_r_str: str = f"{displace_value_y_r:.3f}"
            displace_value_y_g_str: str = f"{displace_value_y_g:.3f}"
            displace_value_y_b_str: str = f"{displace_value_y_b:.3f}"
            tpf_str: str = f"{point_max_level.time_point_from}"
            #ball_zoom_str: str = f"{ball_zoom}"
            #displace_value_x_r_str: str = f"{displace_value_x_r}"
            #displace_value_x_g_str: str = f"{displace_value_x_g}"
            #displace_value_x_b_str: str = f"{displace_value_x_b}"
            #displace_value_y_r_str: str = f"{displace_value_y_r}"
            #displace_value_y_g_str: str = f"{displace_value_y_g}"
            #displace_value_y_b_str: str = f"{displace_value_y_b}"

            commands: list[str] = [
                f"scale@b width '{ball_zoom_str}*in_w'",
                f"scale@b height '{ball_zoom_str}*in_h'",
                f"lut@dx r '{displace_value_x_r_str}*255'",
                f"lut@dx g '{displace_value_x_g_str}*255'",
                f"lut@dx b '{displace_value_x_b_str}*255'",
                f"lut@dy r '{displace_value_y_r_str}*255'",
                f"lut@dy g '{displace_value_y_g_str}*255'",
                f"lut@dy b '{displace_value_y_b_str}*255'",
            ]

            fout.write(f"{tpf_str} " + ", ".join(commands) + ";\n")
            fo_b.write(f"{tpf_str} " + ", ".join(commands[0:2]) + ";\n")
            fo_dx.write(f"{tpf_str} " + ", ".join(commands[2:5]) + ";\n")
            fo_dy.write(f"{tpf_str} " + ", ".join(commands[5:]) + ";\n")

if __name__ == "__main__":
    main()
