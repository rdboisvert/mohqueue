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
    id SERIAL PRIMARY KEY NOT NULL,
    msgq_id INTEGER NOT NULL,
    call_state INTEGER NOT NULL,
    call_id VARCHAR(100) NOT NULL,
    call_from VARCHAR(100) NOT NULL,
    call_tag VARCHAR(100) NOT NULL,
    msgq_time date NOT NULL,
    CONSTRAINT msgqcalls_idx UNIQUE (call_id)
);

INSERT INTO msgqueues (msgq_name, msgq_uri, msgq_mohdir, msgq_mohfile) values ('test','sip:9001@10.211.64.5','/var/build','music_on_hold');