var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
// Init web socket when the page loads
window.addEventListener('load', onload);

function onload(event) {
    initWebSocket();
}

function getReadings(){
    websocket.send("getReadings");
}

function initWebSocket() {
    console.log('Trying to open a WebSocket connection…');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

// When websocket is established, call the getReadings() function
function onOpen(event) {
    console.log('Connection opened');
    getReadings();
}

function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

// Heartbeat Überwachung
let heartbeat = true;
let heartbeatTimeout = null;



function showConnectionLost() {
    const popup = document.getElementById("connection-lost-popup");
    if (popup) popup.style.display = "flex";
}
function hideConnectionLost() {
    const popup = document.getElementById("connection-lost-popup");
    if (popup) popup.style.display = "none";
}

// Function that receives the message from the ESP32 with the readings
function onMessage(event) {
    var data = event.data;
    console.log(event.data);
    var myObj = JSON.parse(event.data);
    var keys = Object.keys(myObj);
    var mode = "Falsche Daten";

    switch (myObj["mode"]){
        case "0":
            var mode = "Taste Drücken!";
            break;
        case "1":
            var mode = "Zum Starten Dribble-Platte auslösen";
            break;
        case "2":
            var mode = "Restzeit";
            break;
        default:
            var mode = "Falsche Daten";
            break;
    }
    document.getElementById("mode").innerHTML = mode;
    document.getElementById("counter").innerHTML = myObj["counter"];
    document.getElementById("countdown").innerHTML = myObj["countdown"];


    hideConnectionLost()
    if (heartbeatTimeout){
        clearTimeout(heartbeatTimeout);
    }
    // Wenn nach 2 Sekunden kein neuer Heartbeat, Verbindung als verloren anzeigen
    heartbeatTimeout = setTimeout(() => {
        heartbeat = false;
        showConnectionLost();
    }, 2000);
    }
