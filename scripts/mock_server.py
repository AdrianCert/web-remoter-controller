import asyncio
from pathlib import Path
from typing import Any, Dict
from uuid import uuid4

from aiohttp import web
from aiohttp_sse import EventSourceResponse, sse_response

routes = web.RouteTableDef()
resurces = Path(__file__).parents[1].joinpath("data")
devices = [True, False, True, False, False]
devices_mac = [0, 2, 3, 4, 5]

event_source: Dict[str, EventSourceResponse] = {}
event_look = asyncio.locks.Lock()


async def safe_run(coro: Any, clue: Any, exp_handler: Any | None = None):
    try:
        return await coro
    except Exception:
        return exp_handler(clue)


def clean_event_client(client):
    event_source.pop(client, None)


async def send_relay_status(relay_index):
    if event_source is None:
        return
    async with event_look:
        if relay_index >= len(devices_mac):
            return
        if devices_mac[relay_index] == 0:
            return
        event_name = "relay_on" if devices[relay_index] else "relay_off"
        event_msg = str(relay_index + 1)
        await asyncio.gather(
            *[
                safe_run(
                    es.send(data=event_msg, event=event_name), key, clean_event_client
                )
                for key, es in event_source.items()
            ]
        )


@routes.get("/")
async def index(request):
    return web.FileResponse(path=resurces.joinpath("index.html"))


@routes.get("/index.js")
async def index_js(request):
    return web.FileResponse(path=resurces.joinpath("index.js"))


@routes.get("/style.css")
async def index_css(request):
    return web.FileResponse(path=resurces.joinpath("style.css"))


@routes.get("/update")
async def update(request):
    # request.get
    if request.query.get("relay") is None:
        dev_state = {"on": True, "off": False}.get(request.query.get("state"))
        for dev_id in range(len(devices_mac)):
            devices[dev_id] = dev_state
            await send_relay_status(dev_id)
        return web.Response(body="OK")

    dev_id = int(request.query.get("relay")) - 1
    dev_state = {"on": True, "off": False}.get(request.query.get("state"))
    devices[dev_id] = dev_state

    if dev_state is None:
        devices_mac[dev_id] = request.query.get("state")

    await send_relay_status(dev_id)
    return web.Response(body="OK")


@routes.get("/relays")
async def stats(request):
    return web.json_response(
        {
            str(dev_id + 1): ("on" if dev_state else "off")
            for dev_id, dev_addr, dev_state in zip(range(5), devices_mac, devices)
            if dev_addr
        }
    )


@routes.get("/events")
async def events(request):
    global event_source

    async with sse_response(request) as resp:
        event_source[uuid4().hex] = resp
        while True:
            for dev_id in range(5):
                await send_relay_status(dev_id)
            await asyncio.sleep(3)


if __name__ == "__main__":
    app = web.Application()
    app.add_routes(routes)
    web.run_app(app)
