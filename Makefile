
WEB?=/var/www/html/rt2
WEB_BIN?=/var/www/bin
DB_DIR?=/var/lib/rt2
DB_FILE=$(DB_DIR)/plan.db
VIS_DB_FILE=$(DB_DIR)/visibility.db

PHP_FILES=*.php
OTHER_FILES=style.css script.js
PY_FILES=gen_obs_json.py
SHELL_FILES=daily_sun_plan.sh

web:
	mkdir -p $(WEB)
	cd web_server && cp -f $(PHP_FILES) $(OTHER_FILES) $(WEB)

script:
	mkdir -p $(WEB_BIN)
	cd web_server && cp -f $(PY_FILES) $(WEB_BIN)
	chmod +x $(WEB_BIN)/$(PY_FILES)

sun:
	mkdir -p $(WEB_BIN)
	cd web_server && cp -f $(SHELL_FILES) $(WEB_BIN)
	chmod +x $(WEB_BIN)/$(SHELL_FILES)

db:
	mkdir -p $(DB_DIR)
	@if [ ! -f $(DB_FILE) ]; then \
		sqlite3 $(DB_FILE) < web_server/schema.sql; \
	fi
	@if [ ! -f $(VIS_DB_FILE) ]; then \
		sqlite3 $(VIS_DB_FILE) < web_server/visibility_schema.sql; \
	fi

install: web script db
