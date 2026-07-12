const sqlInput = document.querySelector("#sql-input");
const runButton = document.querySelector("#run-query-button");
const statusBadge = document.querySelector("#status-badge");
const message = document.querySelector("#message");
const affectedRows = document.querySelector("#affected-rows");
const executionTime = document.querySelector("#execution-time");
const resultTable = document.querySelector("#result-table");
const catalogContent = document.querySelector("#catalog-content");
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

function renderCatalogEmptyState(text) {
    catalogContent.innerHTML = `<div class="empty-state">${escapeHtml(text)}</div>`;
}

function renderBadge(text, className = "") {
    return `<span class="catalog-badge ${className}">${escapeHtml(text)}</span>`;
}

function renderCatalogSection(title, count, bodyHtml, collapsed = false) {
    return `
        <section class="catalog-section ${collapsed ? "collapsed" : ""}">
            <button class="catalog-section-toggle" type="button" data-section-toggle>
                <span>${escapeHtml(title)}</span>
                <span>${count}</span>
            </button>
            <div class="catalog-section-body">
                <ul>${bodyHtml}</ul>
            </div>
        </section>
    `;
}

function renderCatalog(catalog) {
    const tables = catalog.tables || [];

    if (!tables.length) {
        renderCatalogEmptyState("No tables in the catalog yet.");
        return;
    }

    const tablesHtml = tables
        .map((table, tableIndex) => {
            const columns = table.columns || [];
            const indexes = table.indexes || [];

            const columnsHtml = columns
                .map((column) => {
                    const badges = [];

                    if (column.primary_key) {
                        badges.push(renderBadge("PK", "primary"));
                    }

                    if (column.auto_increment) {
                        badges.push(renderBadge("AUTO", "auto"));
                    }

                    return `
                        <li class="catalog-row">
                            <div>
                                <strong>${escapeHtml(column.name)}</strong>
                                <span>${escapeHtml(column.type)}</span>
                            </div>
                            <div class="catalog-badges">${badges.join("")}</div>
                        </li>
                    `;
                })
                .join("");

            const indexesHtml = indexes
                .map((index) => {
                    const badges = [];

                    if (index.primary) {
                        badges.push(renderBadge("PRIMARY", "primary"));
                    }

                    if (index.unique) {
                        badges.push(renderBadge("UNIQUE", "auto"));
                    }

                    return `
                        <li class="catalog-row">
                            <div>
                                <strong>${escapeHtml(index.name)}</strong>
                                <span>${escapeHtml(index.column)}</span>
                            </div>
                            <div class="catalog-badges">${badges.join("")}</div>
                        </li>
                    `;
                })
                .join("");

            return `
                <article class="catalog-table ${tableIndex === 0 ? "" : "collapsed"}">
                    <button class="catalog-table-toggle" type="button" data-table-toggle>
                        <span class="catalog-table-name">${escapeHtml(table.name)}</span>
                        <span class="catalog-table-count">${columns.length} columns</span>
                    </button>

                    <div class="catalog-table-body">
                        <p class="catalog-heap-file">${escapeHtml(table.heap_file)}</p>

                        ${renderCatalogSection(
                            "Columns",
                            columns.length,
                            columnsHtml || "<li class=\"catalog-muted\">No columns</li>"
                        )}

                        ${renderCatalogSection(
                            "Indexes",
                            indexes.length,
                            indexesHtml || "<li class=\"catalog-muted\">No indexes</li>",
                            true
                        )}
                    </div>
                </article>
            `;
        })
        .join("");

    catalogContent.innerHTML = tablesHtml;
}

async function refreshCatalog() {
    try {
        const response = await fetch("/catalog");
        const catalog = await response.json();
        renderCatalog(catalog);
    } catch (error) {
        renderCatalogEmptyState(`Could not load catalog: ${error.message}`);
    }
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

        const backendTime = result.execution_time_ms;

        if (typeof backendTime === "number") {
            executionTime.textContent = 
                `Execution time: ${backendTime.toFixed(2)} ms backend / ${elapsedMs.toFixed(2)} ms total`;
        } else {
            executionTime.textContent = `Execution time: ${elapsedMs.toFixed(2)} ms total`;
        }

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
        refreshCatalog();
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

catalogContent.addEventListener("click", (event) => {
    const tableToggle = event.target.closest("[data-table-toggle]");
    if (tableToggle) {
        tableToggle.closest(".catalog-table").classList.toggle("collapsed");
        return;
    }

    const sectionToggle = event.target.closest("[data-section-toggle]");
    if (sectionToggle) {
        sectionToggle.closest(".catalog-section").classList.toggle("collapsed");
    }
});

renderEmptyState("Run a query to render results.");
refreshCatalog();
