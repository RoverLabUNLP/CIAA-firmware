#!/usr/bin/env python3
"""Consola USB 115200 sin dependencias. No envia comandos de movimiento."""
import argparse
import glob
import os
import select
import sys
import termios
import time
import tty


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Ejemplo: /dev/ttyUSB1")
    parser.add_argument("--duration", type=float, default=0, help="Segundos; 0 hasta Ctrl+C")
    parser.add_argument("--log", help="Archivo para guardar salida recibida")
    parser.add_argument("--send", choices=["d", "a", "?"], help="Consulta inicial opcional")
    args = parser.parse_args()
    ports = sorted(glob.glob("/dev/serial/by-id/*FTDI*if01*"))
    if not args.port:
        if len(ports) != 1:
            parser.error("Indica --port; no se encontro un unico FTDI canal 1 de la CIAA")
        args.port = ports[0]
    fd = None
    old_attrs = None
    log = None
    try:
        fd = os.open(args.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        old_attrs = termios.tcgetattr(fd)
        tty.setraw(fd, termios.TCSANOW)
        attrs = termios.tcgetattr(fd)
        attrs[4] = attrs[5] = termios.B115200
        attrs[2] |= termios.CLOCAL | termios.CREAD
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        log = open(args.log, "wb") if args.log else None
        print(f"Consola {args.port}, 115200 8N1. d=estado, a=auto, ?=ayuda. Ctrl+C sale.", flush=True)
        if args.send:
            os.write(fd, args.send.encode("ascii"))
        started = time.monotonic()
        keyboard = sys.stdin.isatty()
        while not args.duration or time.monotonic() - started < args.duration:
            readers = [fd] + ([sys.stdin] if keyboard else [])
            readable, _, _ = select.select(readers, [], [], 0.1)
            if fd in readable:
                chunk = os.read(fd, 4096)
                if not chunk:
                    raise OSError("Puerto desconectado")
                sys.stdout.buffer.write(chunk)
                sys.stdout.buffer.flush()
                if log:
                    log.write(chunk)
                    log.flush()
            if keyboard and sys.stdin in readable:
                line = sys.stdin.readline()
                if not line:
                    keyboard = False
                else:
                    # El lector solo permite consultas, nunca movimiento/rearme.
                    safe = "".join(c for c in line if c in "da?")
                    if safe:
                        os.write(fd, safe.encode("ascii"))
    except KeyboardInterrupt:
        pass
    except OSError as exc:
        print(f"Error de consola: {exc}", file=sys.stderr)
        return 1
    finally:
        if log:
            log.close()
        if fd is not None:
            try:
                if old_attrs is not None:
                    termios.tcsetattr(fd, termios.TCSANOW, old_attrs)
            finally:
                os.close(fd)
    return 0


if __name__ == "__main__":
    sys.exit(main())
