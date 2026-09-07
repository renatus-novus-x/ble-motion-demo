import sys


path = sys.argv[1]
command = sys.argv[2].encode("ascii")

with open(path, "rb") as source:
    data = source.read()

has_eof = data.endswith(b"\x1a")
body = data[:-1] if has_eof else data
normalized = body.replace(b"\r\n", b"\n").replace(b"\r", b"\n")
lines = [line.strip().lower() for line in normalized.split(b"\n")]

if command.lower() not in lines:
    if body.endswith(b"\r"):
        body += b"\n"
    elif body and not body.endswith(b"\n"):
        body += b"\r\n"
    body += command + b"\r\n"

if has_eof:
    body += b"\x1a"

with open(path, "wb") as destination:
    destination.write(body)
