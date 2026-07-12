CREATE TABLE appointments (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    title STRING,
    appointment_date DATE,
    priority INTEGER,
    completed BOOLEAN
);

INSERT INTO appointments (title, appointment_date, priority, completed)
VALUES ('Database project review', DATE '2026-07-12', 1, FALSE);

INSERT INTO appointments (title, appointment_date, priority, completed)
VALUES ('Write storage notes', DATE '2026-07-13', 2, FALSE);

INSERT INTO appointments (title, appointment_date, priority, completed)
VALUES ('Frontend demo polish', DATE '2026-07-14', 1, TRUE);

INSERT INTO appointments (title, appointment_date, priority, completed)
VALUES ('Index scan benchmark', DATE '2026-07-15', 3, FALSE);

SELECT * FROM appointments;