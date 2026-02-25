
WEB?=/var/www/html/rt2
WEB_BIN?=/var/www/bin
DB_DIR?=/var/lib/rt2
DB_FILE=$(DB_DIR)/plan.db

PHP_FILES=main_page.php process.php get_day_plan.php get_database.php generate_chart.php generate_timeline.php plan_to_db.php config.php connect_db.php
OTHER_FILES=style.css script.js
PY_FILES=gen_obs_json.py

install:
	mkdir -p $(WEB) $(WEB_BIN) $(DB_DIR)

	cd web_server && cp -f $(PHP_FILES) $(OTHER_FILES) $(WEB)
	cd web_server && cp -f $(PY_FILES) $(WEB_BIN)

	chmod +x $(WEB_BIN)/$(PY_FILES)

	@if [ ! -f $(DB_FILE) ]; then \
		sqlite3 $(DB_FILE) < web_server/schema.sql; \
	fi
