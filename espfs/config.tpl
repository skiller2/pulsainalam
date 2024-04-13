<html><head>
<meta charset="UTF-8">
<title>Pulsador</title>
<link rel="stylesheet" type="text/css" href="style.css">
</head>
<body>
<div id="main">
<h1>Configuración general</h1>
<p>Configuración general del dispositivo %serial_number%</p>
<form method="post" id="form_config"action="config.cgi">
	<div>
	<label>Duración de descanso profundo (segundos)</label>
    <input type="text" name="sleep_time" value="%sleep_time%">
	<div/>
	<div>
    <label>Nombre Servidor</label>
    <input type="text" name="server_name" value="%server_name%">
	<div/>
	<div>
    <label>Puerto</label>
    <input type="text" name="server_port" value="%server_port%">
	<div/>
	<div>
    <label>Camino</label>
    <input type="text" name="server_path" value="%server_path%">
	<div/>
	<div>
    <label>Mantiene radio encendida</label>
    <input type="text" name="radio_always_on" value="%radio_always_on%">
	<div/>
        <div>
    <label>Alarma batería (miliVolt)</label>
    <input type="text" name="min_bat" value="%min_bat%">
	<div/>
</form>
<button type="submit" form="form_config" value="Submit">Confirma</button>
<button type="submit" form="form_config" value="reset">Reiniciar</button>
</div>
</body></html>
