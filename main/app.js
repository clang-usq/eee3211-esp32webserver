console.log('Loaded Garden Monitoring System');

const lastTimeCheckbox = document.getElementById("lastTimeCheckbox");
if (lastTimeCheckbox) {
    lastTimeCheckbox.addEventListener("change", checkboxUpdated);
}

/**
 * Weather dashboard data
 * @typedef {Object} weatherData
 * @property {number} humidity - Relative humidity
 * @property {number} humidityReceived
 * @property {number} windSpeed - Wind speed
 * @property {string} windDirection - Wind direction
 * @property {number} windDataReceived
 * @property {number} rain - Rain amount
 * @property {number} rainReceived
 */


/**
 * @param {weatherData} data
 */
function updateDashboard(data) {
    const options = {
        weekday: "short",
        year : "numeric",
        month : "short",
        hour : "numeric",
        minute : "numeric",
        second : "numeric",
        hour12 : false,
    }
    const humidityTime = new Date(data.humidityReceived * 1000);
    const windDataTime =  new Date(data.windDataReceived * 1000);
    const rainTime =  new Date(data.rainReceived * 1000);

    document.getElementById("humidity").textContent = data.humidityReceived ? data.humidity : "--";

    if(!data.humidityReceived){
        document.getElementById('humidityLastUpdated').textContent = "--";
    } else {
        document.getElementById('humidityLastUpdated').textContent = humidityTime.toLocaleString("en-AU", options);
    };

    document.getElementById("windSpeed").textContent = data.windDataReceived ? data.windSpeed : "--";
    document.getElementById("windDirection").textContent = data.windDataReceived ? data.windDirection : "--";
    if(!data.windDataReceived){
        document.getElementById('windDataLastUpdated').textContent = "--";
    } else {
        document.getElementById('windDataLastUpdated').textContent = windDataTime.toLocaleString("en-AU", options);
    };

    document.getElementById("rain").textContent = data.rainReceived ? data.rain : "--";

    if(!data.rainReceived){
        document.getElementById('rainLastUpdated').textContent = "--";
    } else {
        document.getElementById('rainLastUpdated').textContent = rainTime.toLocaleString("en-AU", options);
    };

}

function setStatus(message) {
    document.getElementById("status").textContent = message;
}

// fetch request to API endpoint
async function requestWeather(){
    const response = await fetch("/v1/data/");

    //log an error if status is not 200. 
    if (!response.ok) {
        throw new Error(`Error: ${response.status}`);
    }
    //else return the response parsed as json.
    return response.json();
}

function checkboxUpdated(){
    const showLastUpdated = Boolean(lastTimeCheckbox?.checked);

    document.getElementById('humidityLastUpdated').classList.toggle('hide', !showLastUpdated);
    document.getElementById('rainLastUpdated').classList.toggle('hide', !showLastUpdated);
    document.getElementById('windDataLastUpdated').classList.toggle('hide', !showLastUpdated);

}

async function refreshDashboard() {
    try {
        const response = await requestWeather();
        updateDashboard(response);
        setStatus("Live API data");
    } catch (error) {
        console.error("Failed to fetch weather data", error);
        // updateDashboard(FALLBACK_DATA);
        // setStatus("Offline demo data");
    }
}

refreshDashboard();
setInterval(refreshDashboard, 5000);
