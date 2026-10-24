"""Runs the FreeDV Reporter server for freedvtest.reporter.ReporterServer.

    python3 _reporter_launcher.py <server source folder> <port>

The server's own source is used as is; this only arranges for it to listen
on the given port and to do without its optional link to Discord.
"""

import os
import runpy
import sys
import types


def main():
    source_dir, port = sys.argv[1], int(sys.argv[2])

    # The server imports the Discord library even when its Discord link is
    # turned off (which it is here), so give it something to import.
    discord = types.ModuleType("discord")
    discord.Client = object
    discord.Intents = object
    discord.Embed = object
    sys.modules["discord"] = discord

    # The server always listens on aiohttp's default port. And when stopped,
    # it needn't wait (by default, up to a minute) for whoever's connected to
    # go first: stopping is meant to be as if it had gone away.
    from aiohttp import web
    run_app = web.run_app
    web.run_app = lambda app, **kwargs: run_app(app, **dict(kwargs, port=port, shutdown_timeout=0.1))

    sys.path.insert(0, source_dir)
    sys.argv = [source_dir + "/freedv-reporter.py"]
    runpy.run_path(sys.argv[0], run_name="__main__")

    # The server never closes its database, whose thread would otherwise keep
    # Python running after the server has stopped (until it is killed).
    os._exit(0)


if __name__ == "__main__":
    main()
