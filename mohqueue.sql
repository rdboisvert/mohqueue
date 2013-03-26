INSERT INTO version (table_name, table_version) values ('mohqueues','1');
CREATE TABLE mohqueues (
  id SERIAL PRIMARY KEY NOT NULL,
  name VARCHAR(25),
  uri VARCHAR(100) NOT NULL,
  mohdir VARCHAR(100),
  mohfile VARCHAR(100) NOT NULL,
  debug INTEGER NOT NULL,
  CONSTRAINT mohqueue_uri_idx UNIQUE (uri),
  CONSTRAINT mohqueue_name_idx UNIQUE (name)
);

INSERT INTO version (table_name, table_version) values ('mohqcalls','1');
CREATE TABLE mohqcalls (
  id SERIAL PRIMARY KEY NOT NULL,
  mohq_id INTEGER NOT NULL,
  call_id VARCHAR(100) NOT NULL,
  call_status INTEGER NOT NULL,
  call_from VARCHAR(100) NOT NULL,
  call_contact VARCHAR(100),
  call_time TIMESTAMP WITHOUT TIME ZONE NOT NULL,
  CONSTRAINT mohqcalls_idx UNIQUE (call_id)
);

INSERT INTO mohqueues (name, uri, mohdir, mohfile, debug) values ('test','sip:9001@10.211.64.5','/var/build/MOH','HeWillCall',1);