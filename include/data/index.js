function getDateTime() {
    return new Date().toISOString();
}

function toggleCheckbox(event) {
    element = event.target;
    var xhr = new XMLHttpRequest();
    element.checked = element.checked ? false: true;
    let id = element.id.split("individual-btn-")[1];
    let url = `update?relay=${id}&state=${element.checked? 'off': 'on'}`;
    xhr.open("GET", url, true);
    xhr.send();
}

function toggleCheckboxAll(element) {
    var xhr = new XMLHttpRequest();
    element.checked = element.checked ? false: true;
    let url = `update?state=${element.checked? 'off': 'on'}`;
    xhr.open("GET", url, true);
    xhr.send();
}

function create_relay_btn(container, r_index, r_state) {
    let root = document.createElement('label');
    root.classList = [ 'switch'];
    container.appendChild(root);

    let input = document.createElement('input');
    input.type = "checkbox"
    input.onchange = toggleCheckbox;
    input.id = `individual-btn-${r_index}`;
    input.checked = r_state == "on" ? true: false;
    root.appendChild(input);

    let slider = document.createElement('span');
    slider.classList = ["slider"];
    root.appendChild(slider);

    let title = document.createElement('span');
    title.classList = ['name'];
    title.textContent = `ready #${r_index}`;
    root.appendChild(title);
}

function paint_btns(data) {
    let container = document.getElementById("individual-pannel");
    // return;
    container.innerHTML = "";
    for (const [key, value] of Object.entries(data)) {
        console.log(`${key}: ${value}`);
        create_relay_btn(container, key, value);
    }
}

function main() {
    fetch('/relays').then( r => r.json()).then(paint_btns);
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
            console.log("relay_on", e.data);
            let obj = JSON.parse(e.data);
            let ts_now = getDateTime();
            let ts_ind = document.getElementById("individual-lts");
            let ts_com = document.getElementById("concomitent-lts");
            console.log(`individual-btn-${obj}`)
            console.log(`individual-btn-${e.data}`)
            let btn_ind = document.getElementById(`individual-btn-${obj}`);
            btn_ind.checked = true;
            ts_com.innerHTML = ts_now;
            ts_ind.innerHTML = ts_now;
        },
        false
    );

    source.addEventListener(
        "relay_off",
        (e) => {
            console.log("relay_off", e.data);
            let obj = JSON.parse(e.data);
            let ts_now = getDateTime();
            let ts_ind = document.getElementById("individual-lts");
            let ts_com = document.getElementById("concomitent-lts");
            console.log(`individual-btn-${obj}`)
            console.log(`individual-btn-${e.data}`)
            let btn_ind = document.getElementById(`individual-btn-${obj}`);
            btn_ind.checked = false;
            ts_com.innerHTML = ts_now;
            ts_ind.innerHTML = ts_now;
        },
        false
    );
}
