
WEB?=/var/www/html/rt2
WEB_BIN?=/var/www/bin

PHP_FILES=main_page.php 
OTHER_FILES=style.css script.js
PY_FILES=gen_obs_json.py

install:
	cd web_server && cp -f $(PHP_FILES) $(OTHER_FILES) $(WEB)
	cd web_server && cp -f $(PY_FILES) $(WEB_BIN)/bin
