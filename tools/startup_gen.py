"""Generate the Vertigo startup animation (64x32 per panel, red).

Needs Pillow (pip install pillow).

    python tools/startup_gen.py preview startup-preview.gif
    python tools/startup_gen.py video startup.mp4        (also needs imageio-ffmpeg)
    python tools/startup_gen.py frames expressions/startup

"frames" writes the unique PNGs as "startup (N).png" and prints the frame
sequence to paste into the "startup" expression in animation.json.
"""
import json
import math
import sys
from PIL import Image

W, H = 64, 32
RED = (255, 0, 0)
FRAME_MS = 40

# 9-row cells: caps on rows 0-6, baseline row 6, descenders rows 7-8
FONT = {
    'L': ["#....", "#....", "#....", "#....", "#....", "#....", "#####"],
    'M': ["#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"],
    'P': ["####.", "#...#", "#...#", "####.", "#....", "#....", "#...."],
    'U': ["#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    'W': ["#...#", "#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#"],
    'O': [".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    'S': [".####", "#....", "#....", ".###.", "....#", "....#", "####."],
    'V': ["#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.."],
    'a': [".....", ".....", ".###.", "....#", ".####", "#...#", ".####"],
    'c': [".....", ".....", ".###.", "#....", "#....", "#....", ".###."],
    'd': ["....#", "....#", ".####", "#...#", "#...#", "#...#", ".####"],
    'e': [".....", ".....", ".###.", "#...#", "#####", "#....", ".###."],
    'f': ["..##", ".#..", ".#..", "####", ".#..", ".#..", ".#.."],
    'g': [".....", ".....", ".####", "#...#", "#...#", "#...#", ".####", "....#", ".###."],
    'h': ["#....", "#....", "#.##.", "##..#", "#...#", "#...#", "#...#"],
    'i': ["#", ".", "#", "#", "#", "#", "#"],
    'l': ["#.", "#.", "#.", "#.", "#.", "#.", ".#"],
    'm': [".....", ".....", "##.#.", "#.#.#", "#.#.#", "#.#.#", "#.#.#"],
    'n': [".....", ".....", "#.##.", "##..#", "#...#", "#...#", "#...#"],
    'o': [".....", ".....", ".###.", "#...#", "#...#", "#...#", ".###."],
    'r': ["....", "....", "#.##", "##..", "#...", "#...", "#..."],
    't': [".#..", ".#..", "####", ".#..", ".#..", ".#..", "..##"],
    'u': [".....", ".....", "#...#", "#...#", "#...#", "#..##", ".##.#"],
    'w': [".....", ".....", "#...#", "#...#", "#.#.#", "#.#.#", ".#.#."],
    '.': [".", ".", ".", ".", ".", ".", "#"],
    ',': ["..", "..", "..", "..", "..", "..", ".#", "#."],
    ' ': ["...", "...", "...", "...", "...", "...", "..."],
}
LINE_PITCH = 10

MESSAGES = [
    # (lines, bounce the trailing dots after typing)
    (["Loading", "firmware..."], True),
    (["Munching", "on ram..."], True),
    (["Practicing", "UwU..."], True),
    (["Welcome to", "ProotOS,", "Vertigo"], False),
]


def text_width(s):
    return sum(len(FONT[c][0]) for c in s) + max(len(s) - 1, 0)


def draw_text(img, lines, visible_chars, dot_lift=(0, 0, 0)):
    """Left-aligned, vertically centered. Only the first visible_chars characters are drawn.
    dot_lift raises the three dots of a trailing "..." by that many pixels each."""
    px = img.load()
    block_h = LINE_PITCH * (len(lines) - 1) + 9
    y0 = (H - block_h) // 2
    shown = 0
    for li, line in enumerate(lines):
        if text_width(line) > W - 2:
            raise ValueError(f"'{line}' is too wide for the panel")
        x = 2
        y = y0 + li * LINE_PITCH
        dots_start = len(line) - 3 if line.endswith("...") else len(line)
        for ci, ch in enumerate(line):
            if shown >= visible_chars:
                return
            glyph = FONT[ch]
            lift = dot_lift[ci - dots_start] if ci >= dots_start else 0
            for gy, row in enumerate(glyph):
                for gx, bit in enumerate(row):
                    if bit == '#':
                        px[x + gx, y + gy - lift] = RED
            x += len(glyph[0]) + 1
            shown += 1


def line_points(x0, y0, x1, y1):
    pts = []
    dx, dy = abs(x1 - x0), -abs(y1 - y0)
    sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
    err = dx + dy
    while True:
        pts.append((x0, y0))
        if x0 == x1 and y0 == y1:
            return pts
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x0 += sx
        if e2 <= dx:
            err += dx
            y0 += sy


ZERO_CX, ZERO_CY, ZERO_RX, ZERO_RY = 42, 18, 8, 9


def logo_parts():
    """The logo's strokes as ordered pen paths: the root (with overline), the zero, the slash."""
    root = []
    root += line_points(11, 17, 14, 17)   # flat lead-in, matching the overline
    root += line_points(14, 17, 20, 28)   # down stroke
    root += line_points(20, 28, 27, 2)    # long up stroke
    root += line_points(27, 2, 56, 2)     # overline
    zero = []                             # starting at the top, going clockwise
    for i in range(72):
        a = -math.pi / 2 + 2 * math.pi * i / 72
        zero.append((round(ZERO_CX + ZERO_RX * math.cos(a)), round(ZERO_CY + ZERO_RY * math.sin(a))))
    slash = line_points(34, 28, 50, 9)    # bottom-left to top-right
    return root, zero, slash


def logo_strokes(erasing=False):
    """Splits the logo into strokes that all animate at once.

    Each stroke is an ordered path, revealed (or erased) from its first point
    toward its last. Strokes in the same group are timed to finish together.

    The root runs from its tip to the end of the overline both ways.
    The zero is one loop around the circle with the slash growing or shrinking
    from both ends to meet it. Drawing in, the loop starts and ends where the
    slash crosses it at the bottom left. Erasing, it starts and ends at the
    top-right crossing.
    """
    root, zero, slash = logo_parts()

    def off_ellipse(p):
        return abs(((p[0] - ZERO_CX) / ZERO_RX) ** 2 + ((p[1] - ZERO_CY) / ZERO_RY) ** 2 - 1)

    def nearest_slash(p):
        return min((p[0] - s[0]) ** 2 + (p[1] - s[1]) ** 2 for s in slash)

    upper = [i for i, p in enumerate(zero) if p[1] < ZERO_CY]
    lower = [i for i, p in enumerate(zero) if p[1] >= ZERO_CY]
    z_tr = min(upper, key=lambda i: nearest_slash(zero[i]))
    z_bl = min(lower, key=lambda i: nearest_slash(zero[i]))
    s_bl = min((j for j, p in enumerate(slash) if p[1] >= ZERO_CY), key=lambda j: off_ellipse(slash[j]))
    s_tr = min((j for j, p in enumerate(slash) if p[1] < ZERO_CY), key=lambda j: off_ellipse(slash[j]))

    n = len(zero)
    if erasing:
        zero_group = [
            [zero[(z_tr + k) % n] for k in range(n + 1)],  # one loop from the top-right crossing back to it
            slash[:s_tr + 1],                               # bottom end up to the crossing
            slash[s_tr:][::-1],                             # top end down to the crossing
        ]
    else:
        zero_group = [
            [zero[(z_bl + k) % n] for k in range(n + 1)],  # one loop from the bottom-left crossing back to it
            slash[:s_bl + 1],                               # bottom end up to the crossing
            slash[s_bl:][::-1],                             # top end down to the crossing
        ]
    return [root], zero_group


def logo_frame(t, erasing=False):
    """Visible points at time t (0-1) of the draw or erase animation."""
    pts = []
    for group in logo_strokes(erasing):
        longest = max(len(s) for s in group)
        k = t * longest
        for stroke in group:
            count = int(max(0, min(len(stroke), k - (longest - len(stroke)))))
            pts += stroke[count:] if erasing else stroke[:count]
    return pts


def paint(img, pts):
    px = img.load()
    for x, y in pts:
        for ox in (-1, 0, 1):
            for oy in (-1, 0, 1):
                if 0 <= x + ox < W and 0 <= y + oy < H:
                    px[x + ox, y + oy] = RED


def ms(milliseconds):
    return max(1, round(milliseconds / FRAME_MS))


def build():
    """Returns (unique frames, sequence of 1-based frame numbers, one per FRAME_MS)."""
    frames, seq = [], []

    seen = {}

    def add(img, hold_frames=1):
        # Reuse identical frames so they only take flash space once
        key = img.tobytes()
        if key not in seen:
            frames.append(img)
            seen[key] = len(frames)
        seq.extend([seen[key]] * hold_frames)
        return seen[key]

    add(Image.new("RGB", (W, H)), ms(240))

    # Each dot starts bouncing the moment it is typed and bounces twice.
    # Dots are typed 2 steps apart, so together they make a wave.
    bounce_profile = [1, 2, 3, 2, 1]
    bounce_period = len(bounce_profile) + 4
    bounce_count = 2
    dot_spacing = 2

    for lines, bounce in MESSAGES:
        total = sum(len(l) for l in lines)
        if not bounce:
            for n in range(1, total + 1):
                img = Image.new("RGB", (W, H))
                draw_text(img, lines, n)
                full_id = add(img)
            seq.extend([full_id] * ms(2000))     # 2 seconds
            continue

        # Step at which each character appears: letters one per step, dots slower
        dots_from = total - 3
        appear = list(range(dots_from)) + [dots_from + i * dot_spacing for i in range(3)]
        end = appear[-1] + bounce_period * bounce_count
        for step in range(end):
            visible = sum(1 for a in appear if a <= step)
            lift = []
            for dot in range(3):
                t = step - appear[dots_from + dot]
                if 0 <= t < bounce_period * bounce_count and t % bounce_period < len(bounce_profile):
                    lift.append(bounce_profile[t % bounce_period])
                else:
                    lift.append(0)
            img = Image.new("RGB", (W, H))
            draw_text(img, lines, visible, lift)
            add(img)

    # Every message starts the moment the previous one finishes, so there are
    # no blank gaps. The logo draws in, holds, then erases from the same ends.
    steps = 16
    for s in range(1, steps + 1):
        img = Image.new("RGB", (W, H))
        paint(img, logo_frame(s / steps))
        logo_id = add(img)
    seq.extend([logo_id] * ms(2000))             # hold the finished logo 2 seconds
    for s in range(1, steps + 1):
        img = Image.new("RGB", (W, H))
        paint(img, logo_frame(s / steps, erasing=True))
        add(img)
    return frames, seq


def preview(out):
    frames, seq = build()
    scale, gap = 6, 12
    rendered, durations, last = [], [], None
    for idx in seq:
        if idx == last:
            durations[-1] += FRAME_MS
            continue
        big = frames[idx - 1].resize((W * scale, H * scale), Image.NEAREST)
        canvas = Image.new("RGB", (W * 2 * scale + gap, H * scale), (24, 24, 24))
        canvas.paste(big, (0, 0))
        canvas.paste(big, (W * scale + gap, 0))   # flip_right is false: both panels match
        rendered.append(canvas)
        durations.append(FRAME_MS)
        last = idx
    durations[-1] += 1500
    rendered[0].save(out, save_all=True, append_images=rendered[1:], duration=durations, loop=0)
    print(f"{len(frames)} unique frames, {len(seq) * FRAME_MS / 1000:.1f}s long")


def video(out):
    """Renders an MP4 at one video frame per animation step. Needs imageio-ffmpeg."""
    import subprocess
    import imageio_ffmpeg

    frames, seq = build()
    scale, gap = 10, 20
    size = (W * 2 * scale + gap, H * scale)
    cmd = [
        imageio_ffmpeg.get_ffmpeg_exe(), "-y", "-loglevel", "error",
        "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{size[0]}x{size[1]}",
        "-r", str(1000 // FRAME_MS), "-i", "-",
        "-c:v", "libx264", "-preset", "slow", "-crf", "12", "-tune", "animation",
        "-pix_fmt", "yuv420p", "-movflags", "+faststart", out,
    ]
    encoder = subprocess.Popen(cmd, stdin=subprocess.PIPE)
    rendered = {}
    for idx in seq + [1] * (1000 // FRAME_MS):   # one second of black at the end
        if idx not in rendered:
            big = frames[idx - 1].resize((W * scale, H * scale), Image.NEAREST)
            canvas = Image.new("RGB", size, (24, 24, 24))
            canvas.paste(big, (0, 0))
            canvas.paste(big, (W * scale + gap, 0))
            rendered[idx] = canvas.tobytes()
        encoder.stdin.write(rendered[idx])
    encoder.stdin.close()
    if encoder.wait() != 0:
        raise RuntimeError("ffmpeg failed")
    print(f"{size[0]}x{size[1]}, {len(seq) * FRAME_MS / 1000:.1f}s")


def write_frames(out_dir):
    frames, seq = build()
    for i, f in enumerate(frames, 1):
        f.save(f"{out_dir}/startup ({i}).png")
    print(len(frames))
    print(json.dumps(seq))


if __name__ == "__main__":
    {"preview": preview, "video": video, "frames": write_frames}[sys.argv[1]](sys.argv[2])
