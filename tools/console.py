#!/usr/bin/env python3
"""tools/console.py: consola cómoda para el debug de loader. Solo usa la library
estándar.

Terminal 1:  ./loader --debug tu.so
Terminal 2:  python3 tools/console.py

Habla con resources/loader.sock (lo crea loader en su data_dir) usando los
comandos de texto de src/debug/debug_server.c, y añade autocompletado con Tab,
historial, ayuda por comando, atajos, coincidencias numeradas y nombres
guardados. Escribe help para empezar.
"""
import argparse
import json
import os
import queue
import re
import socket
import sys
import threading
import time

try:
    import readline
except ImportError:
    readline = None

HISTORY_FILE = os.path.expanduser("~/.loader_console_history")
NAMES_FILE = ".loader_console.json"
TYPES = ["i8", "u8", "i16", "u16", "i32", "u32", "f32", "ptr", "str"]
CONDITIONS = ["changed", "same", "up", "down"]
ALIASES = {"f": "find", "n": "next", "l": "results", "r": "read", "w": "write", "d": "dump",
           "c": "resume", "p": "pause", "i": "info", "q": "exit"}

# nombre: (grupo, uso, explicación, ejemplos)
COMMANDS = {
    "find": ("Search", "find TYPE VALUE|any", "Starts a search in the game memory, from the .so to the last thing "
             "allocated. With any it keeps every position, for a value you do not know.",
             ["find i32 100", "find f32 0.5", "find i32 any"]),
    "next": ("Search", "next VALUE|changed|same|up|down", "Keeps the hits that match, comparing with the last time "
             "they were checked. Let the game advance between one next and another.",
             ["next 90", "next changed", "next down"]),
    "results": ("Search", "results [COUNT]", "Shows the hits, numbered. Then you can use #1, #2... as a place in other "
                "commands.", ["results", "results 5"]),
    "read": ("Memory", "read PLACE [TYPE] [COUNT]", "Reads values, one after another.",
             ["read #1", "read life", "read @0x4AD0 u8 4", "read g_score"]),
    "write": ("Memory", "write PLACE [TYPE] VALUE", "Changes a value. The game sees it at once. With #1 or a saved "
              "name, the type is optional.", ["write #1 999", "write 0x6080549c i32 190"]),
    "dump": ("Memory", "dump PLACE [BYTES]", "Shows memory in hexadecimal and as text.",
             ["dump @0x4AB0 32", "dump #1 64"]),
    "watch": ("Watch", "watch NAME PLACE [TYPE]", "Reports every change. In any PLACE, +OFFSET only adds and ->OFFSET "
              "reads a pointer at the current address and adds to it, as in C.",
              ["watch life #1", "watch life g_world->0x10->4 i32", "watch volume @0xC2D4C+0x18 f32"]),
    "unwatch": ("Watch", "unwatch NAME", "Stops following a variable.", ["unwatch life"]),
    "watches": ("Watch", "watches", "Shows the variables you follow, with their values.", ["watches"]),
    "pause": ("Execution", "pause [METHOD]", "Stops the game when the next method returns. With METHOD, every time "
              "that method returns.", ["pause", "pause nativeRender"]),
    "unpause": ("Execution", "unpause METHOD", "Stops pausing when that method returns.", ["unpause nativeRender"]),
    "resume": ("Execution", "resume", "The game continues.", ["resume"]),
    "run": ("Execution", "run [COUNT]", "The game continues, and stops after COUNT methods return.", ["run 1", "run 60"]),
    "info": ("Execution", "info", "Shows where things are, and whether the game is paused.", ["info"]),
    "save": ("Names", "save NAME PLACE [TYPE]", "Saves a place under a name, also for next time, in "
             + NAMES_FILE + ". A position with @ stays the same between runs; a heap address can change.",
             ["save life #1", "save points @0x4AD0 i32"]),
    "forget": ("Names", "forget NAME", "Deletes a saved name.", ["forget life"]),
    "names": ("Names", "names", "Shows the saved names, with their values.", ["names"]),
    "guide": ("Help", "guide", "Explains step by step how to find a variable.", ["guide"]),
    "help": ("Help", "help [COMMAND]", "Shows this list, or the details of a command.", ["help find"]),
    "exit": ("Help", "exit", "Closes the console. The game stays as it was.", ["exit"]),
}
GUIDE = """How to find a variable, for example the life:

  1. Stop the game when the life has a known value:   pause
  2. Search for that value:                           find i32 100
  3. Make it change. Advance with run 60, or with resume and pause.
  4. Filter with the new value:                       next 90
  5. Repeat 3 and 4 until few hits remain.
  6. Check it by changing it:                         write #1 999
  7. Save it and follow it:                           save life #1
                                                      watch life #1

If you do not know the value, start with find i32 any and filter with next down, next same or next up.
If it is a float, use f32. If it does not appear, try i16 or u8.

Places: @0x4AD0 is a position in the .so, 0x6080549c an absolute address, g_score a symbol,
#1 a hit and life a saved name."""
LOCAL = {"help", "guide", "save", "forget", "names", "exit"}


class Console:
    def __init__(self, path, one_shot=False):
        self.path = path
        self.one_shot = one_shot
        self.tty = sys.stdout.isatty() and not one_shot
        self.lines = queue.Queue()
        self.print_lock = threading.Lock()
        self.at_prompt = False
        self.prompt = "> "
        self.paused = False
        self.closed = False
        self.asking = False
        self.hits = []        # las direcciones mostradas, para #1, #2...
        self.hit_total = None
        self.hit_type = "i32"
        self.watched = set()
        self.names = self.load_names()

    # ---------- Salida ----------
    def color(self, code, text):
        return f"\033[{code}m{text}\033[0m" if self.tty else text

    def show(self, text):
        with self.print_lock:
            if self.at_prompt and self.tty:
                typed = readline.get_line_buffer() if readline else ""
                sys.stdout.write("\r\033[K" + text + "\n" + self.prompt + typed)
            else:
                sys.stdout.write(text + "\n")
            sys.stdout.flush()

    # ---------- Conexión ----------
    def connect(self):
        said = False
        while True:
            try:
                self.sock = socket.socket(socket.AF_UNIX)
                self.sock.connect(self.path)
                break
            except OSError:
                self.sock.close()
                if not said:
                    print(f"Waiting for {self.path}. Start the game with: ./loader --debug your.so")
                    said = True
                time.sleep(0.2)
        self.file = self.sock.makefile("r", encoding="utf-8", errors="replace")
        welcome = self.file.readline().strip()
        self.paused = "paused" in welcome
        threading.Thread(target=self.read_loop, daemon=True).start()

    def read_loop(self):
        """Los avisos pueden llegar en cualquier momento. Lo demás es la
        respuesta a un comando."""
        for line in self.file:
            line = line.rstrip("\n")
            if line.startswith("[watch]"):
                self.show(self.color("36", line))
            elif line.startswith("paused after"):
                self.paused = True
                self.update_prompt()
                self.show(self.color("33", line))
            else:
                self.lines.put(line)
        self.closed = True
        self.lines.put(None)
        # Si la consola está esperando lo que escribes, nadie más puede avisar
        # de esto.
        if not self.asking and not self.one_shot:
            sys.stdout.write("\r\033[KThe game has finished.\n")
            sys.stdout.flush()
            self.save_history()
            os._exit(0)

    def ask(self, command):
        """Envía un comando y devuelve su respuesta. Termina con ok, error o
        bye."""
        if self.closed:
            return ["error: the game has finished"]
        self.asking = True
        try:
            self.sock.sendall((command + "\n").encode())
            answer, warned = [], False
            while True:
                try:
                    line = self.lines.get(timeout=3)
                except queue.Empty:
                    if not warned:
                        self.show("Waiting: commands are served when the game returns from a method...")
                        warned = True
                    continue
                if line is None:
                    return answer + ["error: the game has finished"]
                answer.append(line)
                if line == "ok" or line == "bye" or line.startswith("error:"):
                    return answer
        except OSError:
            self.closed = True
            return ["error: the game has finished"]
        finally:
            self.asking = False

    # ---------- Nombres ----------
    def load_names(self):
        try:
            with open(NAMES_FILE, encoding="utf-8") as f:
                return json.load(f)
        except (OSError, ValueError):
            return {}

    def save_names(self):
        with open(NAMES_FILE, "w", encoding="utf-8") as f:
            json.dump(self.names, f, indent=2)

    def place_of(self, token):
        """Traduce #1 o un nombre guardado, también con +8 o ->8 detrás.
        Devuelve el lugar y su tipo, si se sabe. Con algo detrás ya es otra
        variable, así que el tipo deja de valer."""
        head, path = re.fullmatch(r"([^+-]*)(.*)", token).groups()
        match = re.fullmatch(r"#(\d+)", head)
        if match:
            index = int(match.group(1)) - 1
            if not 0 <= index < len(self.hits):
                raise ValueError(f"no hit {head}. Use results.")
            place, kind = self.hits[index], self.hit_type
        elif head in self.names:
            place, kind = self.names[head]["place"], self.names[head].get("type")
        else:
            return token, None
        return place + path, None if path else kind

    # ---------- Comandos ----------
    def local_help(self, topic):
        topic = ALIASES.get(topic, topic)
        if topic:
            if topic not in COMMANDS:
                print(self.color("31", f"No such command: {topic}."))
                return
            _, usage, text, examples = COMMANDS[topic]
            print(usage + "\n\n" + text + "\n\nExamples:")
            for example in examples:
                print("  " + example)
            shortcuts = [a for a, c in ALIASES.items() if c == topic]
            if shortcuts:
                print("\nShortcut: " + ", ".join(shortcuts))
            return
        groups = {}
        for name, (group, usage, _, _) in COMMANDS.items():
            groups.setdefault(group, []).append(usage)
        for group, usages in groups.items():
            print(self.color("1", group))
            for usage in usages:
                print("  " + usage)
        print("\nShortcuts: " + ", ".join(f"{a}={c}" for a, c in ALIASES.items()))
        print("Tab completes, the arrows go through the history and help COMMAND gives the details.")
        print("If this is your first time, type guide.")

    def show_answer(self, answer, numbered=False):
        if numbered:
            self.hits = []
        for line in answer:
            if line == "ok":
                continue
            if line.startswith("error:"):
                print(self.color("31", line))
                continue
            if numbered and re.match(r"0x[0-9a-f]+\s", line):
                self.hits.append(line.split()[0])
                line = f"#{len(self.hits):<3} {line}"
            elif numbered:
                match = re.match(r"(\d+) hits", line)
                if match:
                    self.hit_total = int(match.group(1))
            print(line)

    def run_line(self, text):
        parts = text.split()
        if not parts:
            return True
        command = ALIASES.get(parts[0], parts[0])
        args = parts[1:]
        try:
            if command in LOCAL:
                return self.run_local(command, args)
            if command in ("read", "write", "dump") and args:
                args = self.expand_place(command, args)
            elif command == "watch" and len(args) >= 2:
                place, kind = self.place_of(args[1])
                args[1] = place
                if kind and not any(a in TYPES for a in args[2:]):
                    args.append(kind)
                self.watched.add(args[0])
            elif command == "unwatch" and args:
                self.watched.discard(args[0])
            elif command == "find" and args and args[0] in TYPES:
                self.hit_type = args[0]
        except ValueError as problem:
            print(self.color("31", f"error: {problem}"))
            return True
        answer = self.ask(" ".join([command] + args))
        self.show_answer(answer, numbered=command in ("find", "next", "results"))
        failed = bool(answer) and answer[-1].startswith("error:")
        if command == "pause" and not args and not failed:
            self.paused = True
        elif command in ("resume", "run") and not failed:
            self.paused = False
        return not self.closed

    def expand_place(self, command, args):
        place, kind = self.place_of(args[0])
        rest = args[1:]
        if command == "read" and kind and (not rest or rest[0] not in TYPES):
            rest = [kind] + rest
        if command == "write" and len(rest) == 1:
            if not kind:
                raise ValueError("give the type: write PLACE TYPE VALUE")
            rest = [kind] + rest
        return [place] + rest

    def run_local(self, command, args):
        if command == "exit":
            if self.paused:
                print("The game is still paused. Connect again and use resume to continue.")
            return False
        if command == "help":
            self.local_help(args[0] if args else "")
        elif command == "guide":
            print(GUIDE)
        elif command == "save":
            if len(args) < 2:
                raise ValueError("save NAME PLACE [TYPE]")
            place, kind = self.place_of(args[1])
            kind = args[2] if len(args) > 2 else kind or "i32"
            if kind not in TYPES:
                raise ValueError(f"unknown type: {kind}")
            self.names[args[0]] = {"place": place, "type": kind}
            self.save_names()
            print(f"{args[0]} = {place} {kind}")
        elif command == "forget":
            if not args or self.names.pop(args[0], None) is None:
                raise ValueError("no such name")
            self.save_names()
        elif command == "names":
            if not self.names:
                print("No saved names. Use save NAME PLACE [TYPE].")
            for name, saved in self.names.items():
                answer = self.ask(f"read {saved['place']} {saved['type']}")
                value = " ".join(answer[0].split()[1:]) if answer and answer[0].startswith("0x") else "?"
                print(f"{name:16} {saved['place']:14} {saved['type']:4} {value}")
        return True

    # ---------- Autocompletado ----------
    def choices(self, words, starting_new):
        position = len(words) if starting_new else len(words) - 1
        if position == 0:
            return list(COMMANDS) + list(ALIASES)
        command = ALIASES.get(words[0], words[0])
        places = list(self.names) + [f"#{i + 1}" for i in range(len(self.hits))] + ["@0x"]
        if command == "help":
            return list(COMMANDS)
        if command == "find":
            return TYPES[:7] if position == 1 else ["any"]
        if command == "next":
            return CONDITIONS
        if command in ("read", "dump"):
            return places if position == 1 else TYPES
        if command == "write":
            return places if position == 1 else TYPES if position == 2 else []
        if command == "watch":
            return places if position == 2 else TYPES if position > 2 else []
        if command == "unwatch":
            return list(self.watched)
        if command == "save":
            return places if position == 2 else TYPES if position == 3 else []
        if command == "forget":
            return list(self.names)
        return []

    def complete(self, text, state):
        line = readline.get_line_buffer()
        words = line.split()
        options = [c + " " for c in self.choices(words, line.endswith(" ") or not line) if c.startswith(text)]
        return options[state] if state < len(options) else None

    def uses_hook(self):
        return bool(readline) and "libedit" not in (readline.__doc__ or "")

    def input_started(self):
        """Hasta este momento, readline aún conserva la línea anterior: no hay
        que volver a mostrarla."""
        self.at_prompt = True

    def update_prompt(self):
        state = "paused" if self.paused else "running"
        count = f" {self.hit_total} hits" if self.hit_total is not None else ""
        self.prompt = f"{state}{count}> "

    def load_history(self):
        if not readline:
            return
        readline.set_completer(self.complete)
        readline.set_completer_delims(" ")
        # Python puede venir linkado con GNU readline, como en Fedora, o con
        # libedit.
        if "libedit" in (readline.__doc__ or ""):
            readline.parse_and_bind("bind ^I rl_complete")
        else:
            readline.parse_and_bind("tab: complete")
        if self.uses_hook():
            readline.set_pre_input_hook(self.input_started)
        try:
            readline.read_history_file(HISTORY_FILE)
        except OSError:
            pass

    def save_history(self):
        if readline and not self.one_shot:
            try:
                readline.set_history_length(1000)
                readline.write_history_file(HISTORY_FILE)
            except OSError:
                pass

    def interact(self):
        self.load_history()
        print("Connected. Type help to see the commands, or guide to see how to find a variable.")
        while True:
            self.update_prompt()
            if not self.uses_hook():
                self.at_prompt = True
            try:
                text = input(self.prompt)
            except EOFError:
                print()
                break
            except KeyboardInterrupt:
                print()
                continue
            finally:
                self.at_prompt = False
            if not self.run_line(text):
                break
        self.save_history()


def main():
    parser = argparse.ArgumentParser(description="Debug console for loader")
    default_socket = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "resources", "loader.sock")
    parser.add_argument("socket", nargs="?", default=os.path.normpath(default_socket),
                        help="the socket (default: resources/loader.sock in the repo)")
    parser.add_argument("-c", "--command", action="append", default=[],
                        help="run this command and exit; can be repeated")
    args = parser.parse_args()
    console = Console(args.socket, one_shot=bool(args.command))
    console.connect()
    if args.command:
        for command in args.command:
            if not console.run_line(command):
                break
        return
    console.interact()


if __name__ == "__main__":
    main()
