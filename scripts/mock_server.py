import asyncio
from pathlib import Path

from aiohttp import web
from aiohttp_sse import EventSourceResponse, sse_response

routes = web.RouteTableDef()
resurces = Path(__file__).parents[1].joinpath("data")
devices = [True, False, True, False]
devices_mac = [0, 2, 3, 4]

event_source: EventSourceResponse = None
event_look = asyncio.locks.Lock()


async def send_relay_status(relay_index):
    if not event_source:
        return
    async with event_look:
        if devices_mac[relay_index] == 0:
            return
        event_name = "relay_on" if devices[relay_index] else "relay_off"
        event_msg = str(relay_index + 1)
        await event_source.send(data=event_msg, event=event_name)


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
        event_source = resp
        while True:
            for dev_id in range(5):
                await send_relay_status(dev_id)
            await asyncio.sleep(3)


if __name__ == "__main__":
    app = web.Application()
    app.add_routes(routes)
    web.run_app(app)
