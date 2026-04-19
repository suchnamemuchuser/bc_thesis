
WEB?=/var/www/html/rt2
WEB_BIN?=/var/www/bin
USR_BIN?=~/bin
DB_DIR?=/var/lib/rt2
DB_FILE=$(DB_DIR)/plan.db

PHP_FILES=*.php
OTHER_FILES=style.css script.js history.js
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
	mkdir -p $(USR_BIN)
	cd web_server && cp -f $(SHELL_FILES) $(USR_BIN)
	chmod +x $(USR_BIN)/$(SHELL_FILES)

db:
	mkdir -p $(DB_DIR)
	@if [ ! -f $(DB_FILE) ]; then \
		sqlite3 $(DB_FILE) < web_server/web_db_schema.sql; \
	fi

	chown -R www-data:www-data $(DB_DIR)

	chmod 775 $(DB_DIR)

	chmod 664 $(DB_FILE)

install: web script db
