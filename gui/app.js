// DEBUG VERSION of app.js
let statusEl = document.getElementById('status');
let rowsEl = document.getElementById('rows');
let totalEl = document.getElementById('total');
let progressEl = document.getElementById('progress');
let previewEl = document.getElementById('matrixPreview');

console.log("App.js loaded successfully!"); // Verify JS is running

async function loadProgress() {
    try {
        // Add a timestamp to prevent caching
        let url = 'progress.json?t=' + Date.now();
        let r = await fetch(url);
        
        if (!r.ok) { 
            // If file missing (404), just log it and stay idle
            console.warn("Waiting for progress.json... (404)");
            statusEl.innerText = 'idle'; 
            return; 
        }

        let data = await r.json();
        console.log("Data received:", data); // Log the data to console

        statusEl.innerText = data.status || 'running';
        let rows = data.rows_completed || 0;
        rowsEl.innerText = rows;
        let total = data.total_rows || 0;
        totalEl.innerText = total || '?';

        if (total > 0) {
            let pct = Math.min(100, Math.round((rows / total) * 100));
            progressEl.style.width = pct + '%';
        }

        // Check for results
        let res = await fetch('results.json?t=' + Date.now());
        if (res.ok) {
            let j = await res.json();
            renderMatrixPreview(j.matrix, 12);
            statusEl.innerText = 'done';
        }

    } catch (e) {
        // HERE IS THE FIX: Print the error!
        console.error("CRITICAL ERROR:", e);
        statusEl.innerText = "ERROR: " + e.message;
        statusEl.style.color = "red";
    }
}

function renderMatrixPreview(matrix, limit) {
    if (!matrix) return;
    let N = matrix.length;
    let show = Math.min(limit, N);
    let html = '<table>';
    for (let i = 0; i < show; i++) {
        html += '<tr>';
        for (let j = 0; j < show; j++) {
            html += '<td>' + Number(matrix[i][j]).toFixed(1) + '</td>';
        }
        html += '</tr>';
    }
    html += '</table>';
    previewEl.innerHTML = html;
}

document.getElementById('refreshBtn').addEventListener('click', loadProgress);
// Poll every 800ms
setInterval(loadProgress, 800);
loadProgress();