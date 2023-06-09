import json

import aiohttp

from jarvis.functional_modules.functional_module import functional_module, CallerContext

ROOM_SERVER = "http://127.0.0.1:8888/cmd"


@functional_module(
    name="turn_light",
    description="Turn on/off the light.",
    signature={"target": "Determine which light to operate. Candidates are: 'sun', 'disco', 'welcome'", "on": "Turn on or off, bool type>"})
async def turn_light(context: CallerContext, target: str, on: bool):
    # Do the actual control here, something like this
    # room_id = convert_room_name_to_id(room)
    # await control_unit.toggle_light(room_id, on)

    req_params = {
        "cmd": "lighton" if on else "lightoff",
        "target": target
    }

    try:
        async with aiohttp.ClientSession() as session:
            async with session.post(ROOM_SERVER, data=json.dumps(req_params)) as response:
                resp_obj = await response.json()
        await context.reply_text("The light in " + target + " has turned " + ("on" if on else "off"))
        return "Success"
    except:
        await context.reply_text("Failed to turn " + ("on" if on else "off") + " the light: " + target)
        return "Failed"
    pass
