const sqlInput = document.querySelector("#sql-input");
const runButton = document.querySelector("#run-query-button");
const statusBadge = document.querySelector("#status-badge");
const message = document.querySelector("#message");
const affectedRows = document.querySelector("#affected-rows");
const executionTime = document.querySelector("#execution-time");
const resultTable = document.querySelector("#result-table");
const quickActionButtons = document.querySelectorAll("[data-query]");

function setStatus(label, state) {
    statusBadge.textContent = label;
    statusBadge.className = `status-badge ${state}`;
}

function renderEmptyState(text) {
    resultTable.innerHTML = `<div class="empty-state">${text}</div>`;
}

function escapeHtml(value) {
    return String(value)
        .replaceAll("&", "&amp;")
        .replaceAll("<", "&lt;")
        .replaceAll(">", "&gt;")
        .replaceAll('"', "&quot;")
        .replaceAll("'", "&#039;");
}

function renderTable(columns, rows) {
    if (!columns.length || !rows.length) {
        renderEmptyState("No result rows to display.");
        return;
    }

    const headerHtml = columns
        .map((column) => `<th>${escapeHtml(column)}</th>`)
        .join("");

    const rowsHtml = rows
        .map((row) => {
            const cells = row
                .map((value) => `<td>${escapeHtml(value)}</td>`)
                .join("");

            return `<tr>${cells}</tr>`;
        })
        .join("");

    resultTable.innerHTML = `
        <table>
            <thead>
                <tr>${headerHtml}</tr>
            </thead>
            <tbody>${rowsHtml}</tbody>
        </table>
    `;
}

async function runQuery() {
    const sql = sqlInput.value.trim();

    if (!sql) {
        setStatus("Empty", "error");
        message.textContent = "Write a SQL query first.";
        affectedRows.textContent = "Affected rows: -";
        executionTime.textContent = "Execution time: -";
        renderEmptyState("Nothing has been executed yet.");
        return;
    }

    setStatus("Running", "idle");
    message.textContent = "Executing query...";
    affectedRows.textContent = "Affected rows: -";
    executionTime.textContent = "Execution time: measuring...";

    try {
        const startedAt = performance.now();
        const response = await fetch("/query", {
            method: "POST",
            headers: {
                "Content-Type": "application/json"
            },
            body: JSON.stringify({ sql })
        });
        const elapsedMs = performance.now() - startedAt;

        const result = await response.json();
        executionTime.textContent = `Execution time: ${elapsedMs.toFixed(2)} ms`;

        if (!result.success) {
            setStatus("Error", "error");
            message.textContent = result.error_message || "Query failed.";
            affectedRows.textContent = `Affected rows: ${result.affected_rows ?? 0}`;
            renderEmptyState("The query did not produce rows.");
            return;
        }

        setStatus("Success", "success");
        message.textContent = "Query executed successfully.";
        affectedRows.textContent = `Affected rows: ${result.affected_rows ?? 0}`;
        renderTable(result.columns || [], result.rows || []);
    } catch (error) {
        setStatus("Error", "error");
        message.textContent = `Request failed: ${error.message}`;
        affectedRows.textContent = "Affected rows: -";
        executionTime.textContent = "Execution time: -";
        renderEmptyState("Could not reach the local database server.");
    }
}

runButton.addEventListener("click", runQuery);

quickActionButtons.forEach((button) => {
    button.addEventListener("click", () => {
        sqlInput.value = button.dataset.query;
        //runQuery();
    });
});

renderEmptyState("Run a query to render results.");
