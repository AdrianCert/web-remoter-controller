from pathlib import Path
from aiohttp import web
from datetime import datetime

routes = web.RouteTableDef()
resurces = Path(__file__).parents[1].joinpath('include', 'data')
devices = {
    1: "off",
    2: "on",
    3: "off",
    4: "off",
}


@routes.get("/")
async def index(request):
    return web.FileResponse(path=resurces.joinpath("index.html"))

@routes.get("/index.js")
async def index(request):
    return web.FileResponse(path=resurces.joinpath("index.js"))

@routes.get("/style.css")
async def index(request):
    return web.FileResponse(path=resurces.joinpath("style.css"))


@routes.get("/update")
async def update(request):
    # request.get
    print(request.query.get("relay"))
    print(request.query.get("state"))
    return web.Response(body="OK")

@routes.get("/relays")
async def stats(request):
    return web.json_response(devices)

@routes.get("/")
async def index(request):
    return web.FileResponse(path=Path(__file__).parent.joinpath("index.html"))

app = web.Application()
app.add_routes(routes)
# app.add_routes([web.static("static", path=Path(__file__).parent)])
web.run_app(app)
