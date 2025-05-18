#!python3

import collections
from dataclasses import dataclass
import json
import logging
import math
import os
import random
import subprocess
import typing

FPS: float = 60

SPLICE_LENGTHS: float = 30.0

SCRIPT_DIR: str = os.path.dirname(os.path.abspath(__file__))

WORKING_DIR: str = os.getcwd()

logging.basicConfig(
    filename=os.path.join(SCRIPT_DIR, "__internal.log"),
    filemode="w",  # "w" to restart logfile, default is "a"
    level=logging.DEBUG,
    format="[%(asctime)s] [%(levelname)s] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
)

ASTATS_LEVELS_FILE: str = "_internal_levels.log"
SENDCMD_ALL_FILE: str = "_internal_sendcmd_cmds.log"
SENDCMD_B_FILE: str = "_internal_sendcmd_cmds_b.log"
SENDCMD_DX_FILE: str = "_internal_sendcmd_cmds_dx.log"
SENDCMD_DY_FILE: str = "_internal_sendcmd_cmds_dy.log"


def get_ffmpeg_filter_friendly_joined_path(path: str) -> str:
    return path.replace("\\", "/").replace(":", "\\:")


def os_custom_join(*paths: str) -> str:
    return get_ffmpeg_filter_friendly_joined_path(os.path.join(*paths))


AUDIO_EVALUATION_FILTER_GRAPH: str = (
    f"""
[0:a]
  firequalizer=gain_entry='entry(19, -INF); entry(20, 0); entry(100, 0); entry(101, -INF)',
  astats=length=1/{FPS} : metadata=1 : reset=1 : measure_perchannel=none : measure_overall=Min_level,
  astats=length=1/{FPS} : metadata=1 : reset=1 : measure_perchannel=none : measure_overall=Max_level,
  astats=length=1/{FPS} : metadata=1 : reset=1 : measure_perchannel=none : measure_overall=Peak_level,
  astats=length=1/{FPS} : metadata=1 : reset=1 : measure_perchannel=none : measure_overall=RMS_level,
  ametadata=mode=print : file='{os_custom_join(SCRIPT_DIR, ASTATS_LEVELS_FILE)}'
[TO_OUT]
""".replace(
        "\n", " "
    )
)

VIDEO_CREATION_FILTER_GRAPH: str = (
    f"""
nullsrc=size=1920x1080 : rate={FPS},
  sendcmd=filename='{os_custom_join(SCRIPT_DIR, SENDCMD_DX_FILE)}',
  lut@dx=r='random(1)*255':g='random(1)*255':b='random(1)*255'
[DISPLACE_1_IN];
nullsrc=size=1920x1080 : rate={FPS},
  sendcmd=filename='{os_custom_join(SCRIPT_DIR, SENDCMD_DY_FILE)}',
  lut@dy=r='random(1)*255':g='random(1)*255':b='random(1)*255'
[DISPLACE_2_IN];
[0:v]
  format=rgba,
  scale=flags=16388 : width=1920 : height=1080
[BG_IN];
[2:v]
  format=rgba,
  sendcmd=filename='{os_custom_join(SCRIPT_DIR, SENDCMD_B_FILE)}',
  scale@b=flags=16388 : width=in_w : height=in_h
[CIRCLE_IN];
[3:v]
  format=rgba,
  scale=flags=16388 : width=917 : height=812
[ART_IN];
[1:a]
  showwaves=size=917x387 : rate={FPS} : mode=p2p : draw=full : colors=#FF0000FF|#AA0000FF,
  format=rgba,
  dilation,
  scale=flags=16388 : width=917 : height=387
[waves];
[1:a]
  showfreqs=size=917x387 : rate={FPS} : fscale=log : win_size=4096 : win_func=flattop : colors=#600000FF|#400000FF,
  format=rgba,
  scale=flags=16388 : width=917 : height=387
[freqs];
[BG_IN][freqs]
  overlay=format=auto : alpha=straight : eval=init : x=979 : y=24,
  drawtext=fontfile='C\\:/{os_custom_join('Windows', 'Fonts', 'NotoSans-Regular.ttf')}' : fontsize=60 : textfile=title.txt : fix_bounds=true : fontcolor=#FFFFFFFF : x=1437-(text_w/2) : y=217-(text_h/2)
[bg+freqs+title];
[bg+freqs+title][waves]
  overlay=format=auto : alpha=straight : eval=init : x=979 : y=449
[bg+title+waves];
[bg+title+waves][ART_IN]
  overlay=format=auto : alpha=straight : eval=init : x=24 : y=24
[bg+title+waves+art];
[bg+title+waves+art][CIRCLE_IN]
  overlay=format=auto : alpha=straight : eval=frame : y=964.5-(overlay_h/2) : x=114.5+((1804.5-114.5)*(t/$time))-(overlay_w/2)
[ready-for-output];
[ready-for-output][DISPLACE_1_IN][DISPLACE_2_IN]
  displace=edge=wrap,
  yadif,
  format=pix_fmts=yuv420p : color_spaces=bt709
[VIDEO_OUT]
""".replace(
        "\n", " "
    )
)


def call_subprocess_simple(cmd: list[str]) -> list[str]:
    logging.debug(f"call_subprocess_simple with cmd: `\"{'\" \"'.join(cmd)}\"`")
    result: subprocess.CompletedProcess[str] = subprocess.run(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
    )
    if result.returncode != 0:
        logging.error(f"ffprobe error: {result.stderr.strip()}")
        raise RuntimeError(result.stderr.strip())
    return result.stdout.splitlines()


def call_subprocess(
    cmd: list[str], add_progress: bool = True
) -> typing.Generator[str, None, None]:
    if add_progress:
        cmd.append("-progress")
        cmd.append("pipe:1")
        cmd.append("-nostats")
    logging.debug(f"call_subprocess with cmd: `\"{'\" \"'.join(cmd)}\"`")
    with subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        universal_newlines=True,
        bufsize=1,
    ) as proc:
        for line in proc.stdout:
            line: str = line.strip()
            if line:
                yield line
        proc.wait()
        if proc.returncode != 0:
            if add_progress:
                logging.error(f"subprocess error!")
                raise RuntimeError(f"subprocess error!")
            else:
                std_err_out: list[str] = []
                for err_line in proc.stderr:
                    std_err_out.append(err_line.strip())
                logging.error(f"subprocess error: {'\n'.join(std_err_out)}")
                raise RuntimeError("\n".join(std_err_out))


def get_ffprobe_entry_of_file(filename: str, ffprobe_entry: str) -> float:
    cmd: list[str] = [
        "ffprobe",
        "-select_streams",
        "a:0",
        "-show_entries",
        f"stream={ffprobe_entry}",
        "-of",
        "compact",
        filename,
    ]
    return float(call_subprocess_simple(cmd)[0].split("=")[1])


def get_duration_of_file(filename: str) -> float:
    return get_ffprobe_entry_of_file(filename, "duration")


def get_bitrate_of_file(filename: str) -> float:
    return get_ffprobe_entry_of_file(filename, "bit_rate")


@dataclass
class TimeLevelPoint:
    time_point: float = 0.0
    level_value: float = 0.0


class AstatsCategory:
    def __init__(
        self,
        points: list[TimeLevelPoint] | None,
        min_val: float = 0.0,
        max_val: float = 0.0,
    ):
        if not points:
            points = []
        self.points: list[TimeLevelPoint] = points
        self.min_value: float = min_val
        self.max_value: float = max_val

    def __str__(self) -> str:
        return f"AstatsCategory({self.points}, {self.min_value}, {self.max_value})"

    def __repr__(self) -> str:
        return f"AstatsCategory({self.points}, {self.min_value}, {self.max_value})"

    def get_copy(self, from_i: int, to_i: int, time_offset: float) -> typing.Self:
        ret: AstatsCategory = AstatsCategory(None)
        ret.points = []
        for i in range(len(self.points)):
            point = self.points[i]
            if (i < from_i) or (i > to_i):
                continue
            ret.points.append(
                TimeLevelPoint(point.time_point - time_offset, point.level_value)
            )
        ret.min_value = self.min_value
        ret.max_value = self.max_value
        return ret


def get_copy(
    dictOfStuff: dict[str, AstatsCategory], from_i: int, to_i: int, time_offset: float
) -> dict[str, AstatsCategory]:
    ret: dict[str, AstatsCategory] = {}
    for key in dictOfStuff:
        ret[key] = dictOfStuff[key].get_copy(from_i, to_i, time_offset)
    return ret


def my_mod(a: float, b: float) -> float:
    while a >= b:
        a -= b
    while a < 0:
        a += b
    return a


def time_to_timestr(time: float) -> str:
    seconds: float = my_mod(time, 60)
    minutes = int(my_mod(time / 60, 60))
    hours = int(time / 60 / 60)
    return f"{hours}:{minutes}:{seconds}"


def normalToBallZoom(normal: float) -> float:
    normal = max(0.0, min(1.0, normal))
    return 0.5 + (normal * 0.5)


def normalToDisplaceRandom(normal: float) -> float:
    normal = max(0.0, min(1.0, normal))
    random_value: float = random.uniform(-0.5, 0.5)
    make_it_converge: float = math.pow(
        normal, 2
    )  # values towards 0 are more likely, instead of it being linear
    scale: float = 0.2
    return 0.5 + (random_value * make_it_converge * scale)


def getNormal(value: float, min_val: float, max_val: float) -> float:
    return (value - min_val) / (max_val - min_val)


def main() -> None:
    logging.info(f"Starting spliced video creator...")
    logging.debug(f"SCRIPT_DIR='{SCRIPT_DIR}'")
    logging.debug(f"WORKING_DIR='{WORKING_DIR}'")

    # region Variables to use

    art_path: str = os.path.join(WORKING_DIR, "art.png")
    audio_path: str = os.path.join(WORKING_DIR, "audio.wav")
    bg_path: str = os.path.join(SCRIPT_DIR, "bg.old.png")
    circle_path: str = os.path.join(SCRIPT_DIR, "circle.png")
    output_video_path: str = os.path.join(WORKING_DIR, "youtube.mp4")
    logging.debug(f"art_path='{art_path}'")
    logging.debug(f"audio_path='{audio_path}'")
    logging.debug(f"bg_path='{bg_path}'")
    logging.debug(f"circle_path='{circle_path}'")
    logging.debug(f"output_video_path='{output_video_path}'")

    tmp_filtered_audio_path: str = os.path.join(WORKING_DIR, "__filtered.wav")
    logging.debug(f"tmp_filtered_audio_path='{tmp_filtered_audio_path}'")
    tmp_picture_set_dir: str = os.path.join(WORKING_DIR, "__pictures")
    logging.debug(f"tmp_picture_set_dir='{tmp_picture_set_dir}'")
    if not os.path.isdir(tmp_picture_set_dir):
        os.mkdir(tmp_picture_set_dir)
    tmp_picture_set_path: str = os.path.join(tmp_picture_set_dir, "tmp_%06d.png")
    logging.debug(f"tmp_picture_set_path='{tmp_picture_set_path}'")

    duration: float = get_duration_of_file(audio_path)
    logging.debug(f"duration='{duration}'")
    bitrate: float = get_bitrate_of_file(audio_path)
    logging.debug(f"bitrate='{bitrate}'")

    # endregion Variables to use

    # region Analysing audio

    logging.info(f"evaluating audio.wav...")
    for output in call_subprocess(
        [
            "ffmpeg",
            "-hide_banner",
            "-y",
            "-hwaccel",
            "cuda",
            "-hwaccel_output_format",
            "cuda",
            "-r",
            f"{FPS}",
            "-max_size",
            f"{bitrate} / 8 / {FPS}",
            "-i",
            audio_path,
            "-shortest",
            "-filter_complex",
            AUDIO_EVALUATION_FILTER_GRAPH,
            "-map",
            "[TO_OUT]",
            "-map_metadata",
            "-1",
            "-map_chapters",
            "-1",
            tmp_filtered_audio_path,
        ]
    ):
        logging.debug(f"[evaluation] {output}")
    logging.info(f"evaluated audio.wav")

    # endregion Analysing audio

    # region Parsing analysis data

    logging.info(f"parsing {ASTATS_LEVELS_FILE}...")
    dictOfListOfPoints: dict[str, AstatsCategory] = {}
    with open(os.path.join(SCRIPT_DIR, ASTATS_LEVELS_FILE)) as f_in:
        time_point: float = 0.0
        for line in f_in:
            line = line.strip()
            if ("frame:" in line) and ("pts:" in line) and ("pts_time:" in line):
                # line is e.g. "frame:11059 pts:8127568 pts_time:184.298594"
                parts: list[str] = line.split()
                time_point = float(parts[2].split(":")[1])
            else:
                # line is e.g. "lavfi.astats.Overall.Min_level=-0.000001"
                key_str, value_str = line.split("=")
                category: AstatsCategory = None
                if key_str not in dictOfListOfPoints:
                    category = AstatsCategory(None)
                else:
                    category = dictOfListOfPoints[key_str]
                category.points.append(TimeLevelPoint(time_point, float(value_str)))
                dictOfListOfPoints[key_str] = category
    for key in dictOfListOfPoints:
        dictOfListOfPoints[key].points.sort(key=lambda x: x.time_point)
        dictOfListOfPoints[key].min_value = min(
            dictOfListOfPoints[key].points, key=lambda x: x.level_value
        ).level_value
        dictOfListOfPoints[key].max_value = max(
            dictOfListOfPoints[key].points, key=lambda x: x.level_value
        ).level_value
    logging.info(f"parsed {ASTATS_LEVELS_FILE}")
    # logging.debug(f"dictOfListOfPoints: {dictOfListOfPoints}")

    # endregion Parsing analysis data

    # region Splicing time points into chunks

    listofDictsOfListOfPoints: list[dict[str, AstatsCategory]] = []
    randomDictKey: str = ""
    for key in dictOfListOfPoints:
        randomDictKey = key
    current_time_offset: float = 0.0
    last_offset_index: int = 0
    for i in range(len(dictOfListOfPoints[randomDictKey].points)):
        tp: TimeLevelPoint = dictOfListOfPoints[randomDictKey].points[i]
        if (tp.time_point - current_time_offset) >= SPLICE_LENGTHS:
            listofDictsOfListOfPoints.append(
                get_copy(dictOfListOfPoints, last_offset_index, i, current_time_offset)
            )
            logging.debug(
                f"splice going from {current_time_offset} to {current_time_offset + SPLICE_LENGTHS}:"
            )
            last_offset_index = i - 1
            current_time_offset += SPLICE_LENGTHS
    listofDictsOfListOfPoints.append(
        get_copy(
            dictOfListOfPoints,
            last_offset_index,
            len(dictOfListOfPoints[randomDictKey].points),
            current_time_offset,
        )
    )

    # endregion Splicing time points into chunks

    # region Render chunks into picture sets

    current_time_offset: float = 0.0
    for dictOfListOfPoints in listofDictsOfListOfPoints:
        ts_from: float = max(0.0, current_time_offset - 1)
        ts_to: float = current_time_offset + SPLICE_LENGTHS + 1

        logging.info(f"creating sendcmd files...")
        number_time_points: int = 0
        for key in dictOfListOfPoints:
            number_time_points = len(dictOfListOfPoints[key].points)
        with open(os.path.join(SCRIPT_DIR, SENDCMD_ALL_FILE), "w") as fout, open(
            os.path.join(SCRIPT_DIR, SENDCMD_B_FILE), "w"
        ) as fo_b, open(os.path.join(SCRIPT_DIR, SENDCMD_DX_FILE), "w") as fo_dx, open(
            os.path.join(SCRIPT_DIR, SENDCMD_DY_FILE), "w"
        ) as fo_dy:
            for i in range(number_time_points):
                point_min_level: TimeLevelPoint = dictOfListOfPoints[
                    "lavfi.astats.Overall.Min_level"
                ].points[i]
                point_max_level: TimeLevelPoint = dictOfListOfPoints[
                    "lavfi.astats.Overall.Max_level"
                ].points[i]
                point_peak_level: TimeLevelPoint = dictOfListOfPoints[
                    "lavfi.astats.Overall.Peak_level"
                ].points[i]
                point_rms_level: TimeLevelPoint = dictOfListOfPoints[
                    "lavfi.astats.Overall.RMS_level"
                ].points[i]

                relative_min_level: float = getNormal(
                    point_min_level.level_value,
                    dictOfListOfPoints["lavfi.astats.Overall.Min_level"].min_value,
                    dictOfListOfPoints["lavfi.astats.Overall.Min_level"].max_value,
                )
                relative_max_level: float = getNormal(
                    point_max_level.level_value,
                    dictOfListOfPoints["lavfi.astats.Overall.Max_level"].min_value,
                    dictOfListOfPoints["lavfi.astats.Overall.Max_level"].max_value,
                )
                relative_peak_level: float = getNormal(
                    point_peak_level.level_value,
                    dictOfListOfPoints["lavfi.astats.Overall.Peak_level"].min_value,
                    dictOfListOfPoints["lavfi.astats.Overall.Peak_level"].max_value,
                )
                relative_rms_level: float = getNormal(
                    point_rms_level.level_value,
                    dictOfListOfPoints["lavfi.astats.Overall.RMS_level"].min_value,
                    dictOfListOfPoints["lavfi.astats.Overall.RMS_level"].max_value,
                )

                ball_zoom: float = normalToBallZoom(relative_peak_level)
                displace_value_x_r: float = normalToDisplaceRandom(relative_max_level)
                displace_value_x_g: float = normalToDisplaceRandom(relative_max_level)
                displace_value_x_b: float = normalToDisplaceRandom(relative_max_level)
                displace_value_y_r: float = normalToDisplaceRandom(relative_max_level)
                displace_value_y_g: float = normalToDisplaceRandom(relative_max_level)
                displace_value_y_b: float = normalToDisplaceRandom(relative_max_level)

                # tpf_str: str = f"{point_max_level.time_point - (current_time_offset - ts_from):.3f}"
                # ball_zoom_str: str = f"{ball_zoom:.3f}"
                # displace_value_x_r_str: str = f"{displace_value_x_r:.3f}"
                # displace_value_x_g_str: str = f"{displace_value_x_g:.3f}"
                # displace_value_x_b_str: str = f"{displace_value_x_b:.3f}"
                # displace_value_y_r_str: str = f"{displace_value_y_r:.3f}"
                # displace_value_y_g_str: str = f"{displace_value_y_g:.3f}"
                # displace_value_y_b_str: str = f"{displace_value_y_b:.3f}"
                tpf_str: str = f"{point_max_level.time_point - (current_time_offset - ts_from)}"
                ball_zoom_str: str = f"{ball_zoom}"
                displace_value_x_r_str: str = f"{displace_value_x_r}"
                displace_value_x_g_str: str = f"{displace_value_x_g}"
                displace_value_x_b_str: str = f"{displace_value_x_b}"
                displace_value_y_r_str: str = f"{displace_value_y_r}"
                displace_value_y_g_str: str = f"{displace_value_y_g}"
                displace_value_y_b_str: str = f"{displace_value_y_b}"

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
        logging.info(f"created sendcmd files")

        ts_from_str: str = time_to_timestr(ts_from)
        ts_to_str: str = time_to_timestr(ts_to)
        logging.info(f"rendering picture set from {ts_from_str} to {ts_to_str}...")

        tmp_picture_subset_dir: str = os.path.join(tmp_picture_set_dir, f"{ts_from}-{ts_to}")
        logging.debug(f"tmp_picture_subset_dir='{tmp_picture_subset_dir}'")
        if not os.path.isdir(tmp_picture_subset_dir):
            os.mkdir(tmp_picture_subset_dir)
        tmp_picture_subset_path: str = os.path.join(tmp_picture_subset_dir, "tmp_%06d.png")
        logging.debug(f"tmp_picture_subset_path='{tmp_picture_subset_path}'")

        for output in call_subprocess(
            [
                "ffmpeg",
                "-hide_banner",
                # "-loglevel",
                # "debug",
                "-y",
                "-hwaccel",
                "cuda",
                "-hwaccel_output_format",
                "cuda",
                "-ss",
                ts_from_str,
                "-to",
                ts_to_str,
                "-loop",
                "1",
                "-r",
                f"{FPS}",
                "-i",
                bg_path,
                "-ss",
                ts_from_str,
                "-to",
                ts_to_str,
                "-max_size",
                f"{bitrate} / 8 / {FPS}",
                "-r",
                f"{FPS}",
                "-i",
                audio_path,
                "-ss",
                ts_from_str,
                "-to",
                ts_to_str,
                "-loop",
                "1",
                "-r",
                f"{FPS}",
                "-i",
                circle_path,
                "-ss",
                ts_from_str,
                "-to",
                ts_to_str,
                "-loop",
                "1",
                "-r",
                f"{FPS}",
                "-i",
                art_path,
                "-shortest",
                "-filter_complex",
                VIDEO_CREATION_FILTER_GRAPH.replace("t/$time", f"(t+{ts_from})/{duration}"),
                # "-c:a",
                # "aac",
                # "-ac",
                # "2",
                # "-ar",
                # "48000",
                # "-aac_coder",
                # "fast",
                # "-b:a",
                # "512K",
                "-c:v",  # "-c:v",
                "png",  # "libx264",
                # "-profile:v",
                # "high",
                # "-bf",
                # "2",
                # "-g",
                # f"{FPS / 2}",
                # "-keyint_min",
                # f"{FPS / 2}",
                # "-coder",
                # "ac",
                # "-crf",
                # "1",
                # "-b:v",
                # "20M",
                # "-use_editlist",
                # "0",
                # "-movflags",
                # "+faststart",
                "-r",
                f"{FPS}",
                "-framerate",
                f"{FPS}",
                "-strict",
                "very",
                # "-map",
                # "1:a",
                "-map",
                "[VIDEO_OUT]",
                "-map_metadata",
                "-1",
                "-map_chapters",
                "-1",
                "-start_number",
                f"{round(ts_from * FPS)}",
                tmp_picture_subset_path,
            ]
        ):
            logging.debug(f"[rendering] {output}")
        logging.info(f"rendered picture set from {ts_from_str} to {ts_to_str}")
        current_time_offset += SPLICE_LENGTHS

    # endregion Render chunks into picture sets

    # region Stitch together picture sets

    logging.info(f"stitching picture sets...")
    for output in call_subprocess(
        [
            "ffmpeg",
            "-hide_banner",
            # "-loglevel",
            # "debug",
            "-y",
            "-hwaccel",
            "cuda",
            "-hwaccel_output_format",
            "cuda",
            "-r",
            f"{FPS}",
            "-i",
            tmp_picture_set_path,
            "-max_size",
            f"{bitrate} / 8 / {FPS}",
            "-r",
            f"{FPS}",
            "-i",
            audio_path,
            "-c:a",
            "aac",
            "-ac",
            "2",
            "-ar",
            "48000",
            "-aac_coder",
            "fast",
            "-b:a",
            "512K",
            "-c:v",
            "libx264",
            "-profile:v",
            "high",
            "-pix_fmt",
            "yuv420p",
            "-bf",
            "2",
            "-g",
            f"{FPS / 2}",
            "-keyint_min",
            f"{FPS / 2}",
            "-coder",
            "ac",
            "-crf",
            "1",
            "-b:v",
            "20M",
            "-use_editlist",
            "0",
            "-movflags",
            "+faststart",
            "-r",
            f"{FPS}",
            "-framerate",
            f"{FPS}",
            "-strict",
            "very",
            "-map_metadata",
            "-1",
            "-map_chapters",
            "-1",
            output_video_path,
        ],
        False,
    ):
        logging.debug(f"[stitching] {output}")
    logging.info(f"stitched picture sets")

    # endregion Stitch together picture sets


if __name__ == "__main__":
    main()
