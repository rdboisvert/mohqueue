INSERT INTO version (table_name, table_version) values ('msgqueues','1');
CREATE TABLE msgqueues (
    id SERIAL PRIMARY KEY NOT NULL,
    msgq_name VARCHAR(25),
    msgq_uri VARCHAR(100) NOT NULL,
    msgq_mohdir VARCHAR(100),
    msgq_mohfile VARCHAR(100) NOT NULL,
    CONSTRAINT msgqueue_idx UNIQUE (msgq_uri)
);

INSERT INTO version (table_name, table_version) values ('msgqcalls','1');
CREATE TABLE msgqcalls (
    msgq_id INTEGER NOT NULL,
    msgc_uri VARCHAR(100) NOT NULL,
    msgq_time date NOT NULL,
    CONSTRAINT msgqcalls_idx UNIQUE (msgc_uri)
);
