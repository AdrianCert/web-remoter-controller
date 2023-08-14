var btn_states = {};

function getDateTime() {
    return new Date().toISOString();
}

function toggleCheckbox(event) {
    element = event.target;
    var xhr = new XMLHttpRequest();
    element.checked = element.checked ? false : true;
    let id = element.id.split("individual-btn-")[1];
    let url = `update?relay=${id}&state=${element.checked ? 'off' : 'on'}`;
    xhr.open("GET", url, true);
    xhr.send();
}

function toggleCheckboxAll(element) {
    var xhr = new XMLHttpRequest();
    element.checked = element.checked ? false : true;
    let url = `update?state=${element.checked ? 'off' : 'on'}`;
    xhr.open("GET", url, true);
    xhr.send();
}

function create_relay_btn(container, r_index, r_state) {
    let root = document.createElement('label');
    root.classList = ['switch'];
    container.appendChild(root);

    let input = document.createElement('input');
    input.type = "checkbox"
    input.onchange = toggleCheckbox;
    input.id = `individual-btn-${r_index}`;
    root.appendChild(input);

    let slider = document.createElement('span');
    slider.classList = ["slider"];
    root.appendChild(slider);

    let title = document.createElement('span');
    title.classList = ['name'];
    title.textContent = `ready #${r_index}`;
    root.appendChild(title);

    relay_btn_state_update(r_index, r_state);
}

function paint_btns(data) {
    let container = document.getElementById("individual-pannel");
    container.innerHTML = "";

    for (const [key, value] of Object.entries(data)) {
        create_relay_btn(container, key, value);
    }
}

function relay_btn_state_update(dev_id, state) {
    let ts_now = getDateTime();
    let ts_ind = document.getElementById("individual-lts");
    let ts_com = document.getElementById("concomitent-lts");
    let btn_ind = document.getElementById(`individual-btn-${dev_id}`);
    let valid_state = ["on", "off"].includes(state);

    if (btn_ind == null) {
        sync_relays();
        return;
    }

    btn_ind.checked = state == "on" ? true : false;
    btn_states[dev_id] = btn_ind.checked;
    btn_ind.disabled = !valid_state;
    btn_ind.parentElement.classList = valid_state ? ["switch"] : ["switch", "offline"];

    let tb_com_state = true;
    for (const [key, value] of Object.entries(btn_states)) {
        if ( value == false) {
            tb_com_state = false;
        }
    }

    document.getElementById("concomitent-btn").checked = tb_com_state;

    ts_com.innerHTML = ts_now;
    ts_ind.innerHTML = ts_now;
}

function sync_relays() {
    fetch('/relays').then(r => r.json()).then(paint_btns);
}

function main() {
    sync_relays();
}

document.addEventListener('DOMContentLoaded', main);

if (!!window.EventSource) {
    var source = new EventSource("/events");

    source.addEventListener(
        "open",
        (e) => {
            console.log("Events Connected");
        },
        false
    );
    source.addEventListener(
        "error",
        (e) => {
            if (e.target.readyState != EventSource.OPEN) {
                console.log("Events Disconnected");
            }
        },
        false
    );

    source.addEventListener(
        "message",
        (e) => {
            console.log("message", e.data);
        },
        false
    );

    source.addEventListener(
        "relay_on",
        (e) => {
            relay_btn_state_update(e.data, "on");
        },
        false
    );

    source.addEventListener(
        "relay_off",
        (e) => {
            relay_btn_state_update(e.data, "off");
        },
        false
    );
}
