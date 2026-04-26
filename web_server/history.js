let currentPage = 1;
let currentFilter = 'all';

document.addEventListener('DOMContentLoaded', () => {
    const filterBtns = document.querySelectorAll('.filter-btn');

    filterBtns.forEach(btn => {
        btn.addEventListener('click', (e) => {
            filterBtns.forEach(b => b.classList.remove('active'));
            e.target.classList.add('active');

            currentFilter = e.target.getAttribute('data-filter');
            currentPage = 1; 
            fetchAndRenderTable();
        });
    });

    fetchAndRenderTable();
});

async function fetchAndRenderTable() {
    const tableBody = document.getElementById('history-table-body');
    tableBody.innerHTML = '<tr><td colspan="7" style="text-align: center; padding: 15px;">Loading data...</td></tr>';

    try {
        // Limit is no longer needed in the URL
        const url = `get_plans.php?filter=${currentFilter}&page=${currentPage}`;
        const response = await fetch(url);
        const result = await response.json();

        if (!result.success) throw new Error(result.error);

        renderRows(result.data);
        renderPagination(result.pagination.totalPages);

    } catch (error) {
        tableBody.innerHTML = `<tr><td colspan="7" style="color: red; text-align:center;">Error loading data: ${error.message}</td></tr>`;
    }
}

function renderRows(plans) {
    const tableBody = document.getElementById('history-table-body');
    tableBody.innerHTML = '';

    if (plans.length === 0) {
        tableBody.innerHTML = '<tr><td colspan="7" style="text-align: center; padding: 15px;">No recordings found.</td></tr>';
        return;
    }

    const nowUnix = Math.floor(Date.now() / 1000);
    const localeOptions = { year: 'numeric', month: 'numeric', day: 'numeric', hour: '2-digit', minute: '2-digit' };

    plans.forEach(plan => {
        const tr = document.createElement('tr');
        tr.style.borderTop = '1px solid #ddd';

        const isFuture = plan.rec_start_time > nowUnix;
        const isPast = plan.end_time < nowUnix;

        let statusHtml;

        if (isPast && !plan.has_files) {
            statusHtml = '<span style="color: gray">Empty</span>';
        } else if (isPast) {
            statusHtml = '<span style="color: #06d6a0;">Completed</span>';
        } else if (isFuture) {
            statusHtml = '<span style="color: #ffc43d;">Upcoming</span>';
        } else {
            statusHtml = '<span style="color: #06d6a0;">In progress</span>';
        }

        let iconHtml = '';
        if (plan.has_files) {
            iconHtml = `<a href="${plan.dir_url}" target="_blank" style="text-decoration: none;" title="Browse Files">&#128193;</a>`;
        }

        tr.innerHTML = `
            <td style="padding: 8px;"><strong>${plan.object_name}</strong></td>
            <td style="padding: 8px;">${plan.id}</td>
            <td style="padding: 8px;">${new Date(plan.obs_start_time * 1000).toLocaleString("cs-CZ", localeOptions)}</td>
            <td style="padding: 8px;">${new Date(plan.rec_start_time * 1000).toLocaleString("cs-CZ", localeOptions)}</td>
            <td style="padding: 8px;">${new Date(plan.end_time * 1000).toLocaleString("cs-CZ", localeOptions)}</td>
            <td style="padding: 8px;">${statusHtml}</td>
            <td style="padding: 8px; text-align: center; vertical-align: middle;">${iconHtml}</td>
        `;

        tableBody.appendChild(tr);
    });
}

function renderPagination(totalPages) {
    const controls = document.getElementById('pagination-controls');
    controls.innerHTML = '';

    if (totalPages <= 1) return;

    const prevBtn = document.createElement('button');
    prevBtn.innerText = 'Previous';
    prevBtn.disabled = currentPage === 1;
    prevBtn.onclick = () => { currentPage--; fetchAndRenderTable(); };
    
    const info = document.createElement('span');
    info.innerText = ` Page ${currentPage} of ${totalPages} `;

    const nextBtn = document.createElement('button');
    nextBtn.innerText = 'Next';
    nextBtn.disabled = currentPage === totalPages;
    nextBtn.onclick = () => { currentPage++; fetchAndRenderTable(); };

    controls.appendChild(prevBtn);
    controls.appendChild(info);
    controls.appendChild(nextBtn);
}