INSERT INTO version (table_name, table_version) values ('mohqueues','1');
CREATE TABLE mohqueues (
  id SERIAL PRIMARY KEY NOT NULL,
  mohq_name VARCHAR(25),
  mohq_uri VARCHAR(100) NOT NULL,
  mohq_mohdir VARCHAR(100),
  mohq_mohfile VARCHAR(100) NOT NULL,
  CONSTRAINT mohqueue_idx UNIQUE (mohq_uri)
);

INSERT INTO version (table_name, table_version) values ('mohqcalls','1');
CREATE TABLE mohqcalls (
  id SERIAL PRIMARY KEY NOT NULL,
  mohq_id INTEGER NOT NULL,
  call_state INTEGER NOT NULL,
  call_id VARCHAR(100) NOT NULL,
  call_from VARCHAR(100) NOT NULL,
  call_via VARCHAR(1024) NOT NULL,
  call_tag VARCHAR(100) NOT NULL,
  mohq_time date NOT NULL,
  CONSTRAINT mohqcalls_idx UNIQUE (call_id)
);

INSERT INTO mohqueues (mohq_name, mohq_uri, mohq_mohdir, mohq_mohfile) values ('test','sip:9001@10.211.64.5','/var/build','music_on_hold');