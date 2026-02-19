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

document.getElementById('plan-form').addEventListener('submit', async function(e) {
    e.preventDefault();

    const btn = document.getElementById('submit-btn');
    const resultContainer = document.getElementById('result-container');
    const planGraph = document.getElementById('plan-graph');
    const timelineGraph = document.getElementById('timeline-graph');
    

    btn.disabled = true;
    btn.innerText = "Calculating...";

    const formData = new FormData(this);

    const targetDate = formData.get('date') || '';

    try {
        const [processRes, planRes] = await Promise.all([
            fetch('process.php', { method: 'POST', body: formData }),
            fetch(`get_day_plan.php?date=${encodeURIComponent(targetDate)}`)
        ]);

        const processData = await processRes.json();
        const planData = await planRes.json();

        if (processData.error) throw new Error("Process Error: " + processData.error);
        if (!planData.success) throw new Error("Plan Error: " + planData.error);

        const [chartRes, timelineRes] = await Promise.all([
            fetch('generate_chart.php', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(processData.targets)
            }),
            fetch('generate_timeline.php', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    targetDate: targetDate, 
                    plans: planData.data
                })
            })
        ]);

        if (!chartRes.ok) throw new Error("Chart generation failed");
        if (!timelineRes.ok) throw new Error("Timeline generation failed");

        const chartBlob = await chartRes.blob();
        const timelineBlob = await timelineRes.blob();

        if (planGraph) {
            planGraph.src = URL.createObjectURL(chartBlob);
        }
        
        if (timelineGraph) {
            timelineGraph.src = URL.createObjectURL(timelineBlob);
        }

        const list = document.getElementById('windows-list');
        list.innerHTML = "";

        const baseDateObj = new Date(processData.date + 'T00:00:00');
        const baseUnixTime = baseDateObj.getTime() / 1000;

        processData.targets.forEach(target => {
            const windowsArray = Array.isArray(target.windows) ? target.windows : [target.windows];

            windowsArray.forEach(window => {
                const li = document.createElement('li');

                let startMin = timeToMinutes(window.start);
                let endMin = timeToMinutes(window.end);
                if (endMin < startMin) endMin += 1440;

                const sliderHTML = `
                <strong>${target.name}</strong> (${target.location}):
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
                        object_name: target.name,
                        location: target.location,
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
        });

        const planList = document.getElementById('plan-list');
        planList.innerHTML = "";

        planData.data.forEach(recording => {
            const planLi = document.createElement('li');

            const planHTML = `
            Target: ${recording.object_name}<br>
            OST - RST - ET<br>
            ${recording.obs_start_time} - ${recording.rec_start_time} - ${recording.end_time}<br>
            `;

            planLi.innerHTML = planHTML;
            planList.appendChild(planLi);
        });

        document.getElementById('result-container').style.display = 'block';
    }
    catch (error)
    {
        console.error(error);
        alert(error.message);
    }
    finally
    {
        btn.disabled = false;
        btn.innerText = "Generate visibility";
    }
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