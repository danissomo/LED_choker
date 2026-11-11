
function updateEffectVisibility() {
    const effect = document.getElementById('effect').value;

    const settings = {
        color: false,
        background: false,
        pixel: false,
        trail: false
    };

    switch (effect) {

        case 'color':
        case 'breathing':
        case 'twinkle':
            settings.color = true;
            break;

        case 'strobe':
            settings.color = true;
            settings.background = true;
            break;

        case 'scanner':
            settings.background = true;
            settings.pixel = true;
            settings.trail = true;
            break;
        case 'running':
            settings.background = true;
            settings.pixel = true;
            settings.trail = true;
            break;
    }

    document.getElementById('colorSetting').style.display =
        settings.color ? 'block' : 'none';

    document.getElementById('backgroundSetting').style.display =
        settings.background ? 'block' : 'none';

    document.getElementById('pixelSetting').style.display =
        settings.pixel ? 'block' : 'none';

    document.getElementById('trailSetting').style.display =
        settings.trail ? 'block' : 'none';
}

function updateSettings() {
    const params = new URLSearchParams({
        effect: document.getElementById('effect').value,
        brightness: document.getElementById('brightness').value,
        speed: document.getElementById('speed').value,

        color: document.getElementById('color').value.substring(1),
        bg: document.getElementById('backgroundColor').value.substring(1),
        pixel: document.getElementById('pixelColor').value.substring(1),

        trail: document.getElementById('trail').value
    });

    fetch('/api/set?' + params.toString())
        .catch(console.error);
}

async function updateStatus() {
    try {
        const response = await fetch('/api/status');
        const data = await response.json();

        document.getElementById('percent').innerText =
            data.percent + '%';

        document.getElementById('voltage').innerText =
            data.voltage.toFixed(2) + ' V';

        document.getElementById('batteryFill').style.width =
            data.percent + '%';

        const fill = document.getElementById('batteryFill');

        fill.style.background =
            data.percent < 20 ? '#ed6464' :
            data.percent < 50 ? '#e9bd5b' :
            '#65d99b';

    } catch (e) {
        console.log(e);
    }
}

updateStatus();

setInterval(updateStatus, 2000);

updateEffectVisibility();