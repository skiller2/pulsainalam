<html><head>
<meta charset="UTF-8">
<title>Pulsador</title>
<link rel="stylesheet" type="text/css" href="style.css">
</head>
<body>
<div id="main">
<h1>Firmware</h1>
<p>Upgrade de firmware de equipo %serial_number%</p>
<p>Este pedazo se lo come</p>
<form method="post" id="form_flash" action="flash.cgi">
	<div>
		<label>URL de descarga de flash</label>
	    <input type="text" name="ota_uri" value="%ota_uri%">
	<div/>
	<button type="submit" form="form_flash" name="boton" value="flashear">Flashear</button>
	<button type="submit" form="form_flash" name="boton" value="graba">Grabar</button>
	<button type="submit" value="reset">Reiniciar</button>
</form>
</div>
</body></html>
