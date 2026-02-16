let rowCount = 5;

function addRow() {
    const tbody = document.getElementById('targets-body');
    const row = document.createElement('tr');
    
    row.innerHTML = `
        <td><input type="text" name="targets[${rowCount}][name]" placeholder="tgt name" required></td>
        <td>
            <select name="targets[${rowCount}][type]">
                <option value="solar">Solar System</option>
                <option value="deep" selected>Deep Space</option>
            </select>
        </td>
        <td><input type="checkbox" name="targets[${rowCount}][enabled]" checked></td>
        <td><button type="button" onclick="this.closest('tr').remove()">Remove</button></td>
    `;
    
    tbody.appendChild(row);
    rowCount++;
}



document.getElementById('plan-form').addEventListener('submit', function(e) {
    e.preventDefault();
    const btn = document.getElementById('submit-btn');
    btn.disabled = true;
    btn.innerText = "Calculating...";

    const formData = new FormData(this);

    fetch('process.php', {
            method: 'POST',
            body: formData
        })
        .then(response => response.json())
        .then(data => {
            btn.disabled = false;
            btn.innerText = "Generate Plan";

            if (data.error) {
                alert("Error: " + data.error);
            } else {
                document.getElementById('result-container').style.display = 'block';
                if (document.getElementById('plan-graph')) {
                    document.getElementById('plan-graph').src = './' + data.image + '?t=' + new Date().getTime();
                }

                const list = document.getElementById('windows-list');
                list.innerHTML = "";

                const baseDateObj = new Date(data.date + 'T00:00:00');
                const baseUnixTime = baseDateObj.getTime() / 1000;

                for (const [name, info] of Object.entries(data.objects)) {
                    const windowsArray = Array.isArray(info.windows) ? info.windows : [info.windows];

                    windowsArray.forEach(window => {
                        const li = document.createElement('li');

                        let startMin = timeToMinutes(window.start);
                        let endMin = timeToMinutes(window.end);
                        if (endMin < startMin) endMin += 1440;

                        const sliderHTML = `
                        <strong>${name}</strong> (${info.location}):
                        <div class="range_container">
                            <div class="sliders_control">
                                <input class="fromSlider" type="range" value="${startMin}" min="${startMin}" max="${endMin}"/>
                                <input class="toSlider" type="range" value="${endMin}" min="${startMin}" max="${endMin}"/>
                            </div>
                            <div class="form_control">
                                <div class="form_control_container">
                                    <div class="form_control_container__time">Start</div>
                                    <input class="form_control_container__time__input fromInput" type="text" value="${window.start}"/>
                                </div>
                                <div class="form_control_container">
                                    <div class="form_control_container__time">End</div>
                                    <input class="form_control_container__time__input toInput" type="text" value="${window.end}"/>
                                </div>
                            </div>
                        </div>
                        <button class="save-btn">Plan</button>
                        `;

                        li.innerHTML = sliderHTML;
                        list.appendChild(li);

                        const fromSlider = li.querySelector('.fromSlider');
                        const toSlider = li.querySelector('.toSlider');
                        const fromInput = li.querySelector('.fromInput');
                        const toInput = li.querySelector('.toInput');
                        const saveBtn = li.querySelector('.save-btn');

                        fillSlider(fromSlider, toSlider, '#C6C6C6', '#25daa5', toSlider);
                        setToggleAccessible(toSlider);

                        fromSlider.oninput = () => controlFromSlider(fromSlider, toSlider, fromInput);
                        toSlider.oninput = () => controlToSlider(fromSlider, toSlider, toInput);
                        fromInput.onchange = () => controlFromInput(fromSlider, fromInput, toInput, toSlider);
                        toInput.onchange = () => controlToInput(toSlider, fromInput, toInput, toSlider);
                        
                        saveBtn.addEventListener('click', () => {
                            const currentStartUnix = baseUnixTime + (parseInt(fromSlider.value) * 60);
                            const currentEndUnix = baseUnixTime + (parseInt(toSlider.value) * 60);

                            const payload = {
                                object_name: name,
                                location: info.location,
                                rec_start_time: currentStartUnix,
                                end_time: currentEndUnix
                            };

                            saveBtn.innerText = "Saving...";
                            saveBtn.disabled = true;

                            fetch('plan_to_db.php', {
                                method: 'POST',
                                headers: { 'Content-Type': 'application/json' },
                                body: JSON.stringify(payload)
                            })
                            .then(res => res.json())
                            .then(response => {
                                if(response.success) {
                                    saveBtn.innerText = "Saved!";
                                    fromSlider.disabled = true;
                                    toSlider.disabled = true;
                                } else {
                                    alert("Error: " + response.error);
                                    saveBtn.innerText = "Plan";
                                    saveBtn.disabled = false;
                                }
                            })
                            .catch(err => {
                                console.error("Network Error:", err);
                                alert("Failed to save. Check console.");
                                saveBtn.innerText = "Plan";
                                saveBtn.disabled = false;
                            });
                        });
                    });
                }
            }
        })
        .catch(error => {
            console.error('Fetch Error:', error);
            btn.disabled = false;
            btn.innerText = "Generate Plan";
            alert("A system error occurred.");
        });
});

function controlFromSlider(fromSlider, toSlider, fromInput) {
    const [from, to] = getParsed(fromSlider, toSlider);
    if (from > to) {
        fromSlider.value = to;
        fromInput.value = minutesToTime(to);
    } else {
        fromInput.value = minutesToTime(from);
    }
    fillSlider(fromSlider, toSlider, '#C6C6C6', '#25daa5', toSlider);
}

function controlToSlider(fromSlider, toSlider, toInput) {
    const [from, to] = getParsed(fromSlider, toSlider);
    setToggleAccessible(toSlider);
    if (from <= to) {
        toSlider.value = to;
        toInput.value = minutesToTime(to);
    } else {
        toInput.value = minutesToTime(from);
        toSlider.value = from;
    }
    fillSlider(fromSlider, toSlider, '#C6C6C6', '#25daa5', toSlider);
}

function controlFromInput(fromSlider, fromInput, toInput, controlSlider) {
    const val = timeToMinutes(fromInput.value);
    fromSlider.value = val;
    controlFromSlider(fromSlider, controlSlider, fromInput);
}

function controlToInput(toSlider, fromInput, toInput, controlSlider) {
    const val = timeToMinutes(toInput.value);
    toSlider.value = val;
    controlToSlider(controlSlider, toSlider, toInput);
}

function getParsed(currentFrom, currentTo) {
    const from = parseInt(currentFrom.value, 10);
    const to = parseInt(currentTo.value, 10);
    return [from, to];
}

function fillSlider(from, to, sliderColor, rangeColor, controlSlider) {
    const rangeDistance = to.max - to.min;
    const fromPosition = from.value - to.min;
    const toPosition = to.value - to.min;
    
    const startP = (fromPosition / rangeDistance) * 100;
    const endP = (toPosition / rangeDistance) * 100;

    controlSlider.style.background = `linear-gradient(
      to right,
      ${sliderColor} 0%,
      ${sliderColor} ${startP}%,
      ${rangeColor} ${startP}%,
      ${rangeColor} ${endP}%, 
      ${sliderColor} ${endP}%, 
      ${sliderColor} 100%)`;
}

function setToggleAccessible(currentTarget) {
    if (Number(currentTarget.value) <= 0) {
        currentTarget.style.zIndex = 2;
    } else {
        currentTarget.style.zIndex = 0;
    }
}

function timeToMinutes(timeStr) {
    if (!timeStr) return 0;
    const [h, m] = timeStr.split(':').map(Number);
    return h * 60 + m;
}

function minutesToTime(totalMin) {
    let normalized = totalMin % 1440;
    const h = Math.floor(normalized / 60).toString().padStart(2, '0');
    const m = (normalized % 60).toString().padStart(2, '0');
    return `${h}:${m}`;
}