<html><head><title>Pulsador</title>
<link rel="stylesheet" type="text/css" href="style.css">
</head>
<body>
<div id="main">
<h1>Configuración general</h1>
<p>Configuración general del dispositivo %serial_number%</p>
<form method="post" id="form_config"action="config.cgi">
      <label>Duración de descanso profundo (segundos)</label>
      <input type="text" name="sleep_time" value="%sleep_time%">
</form>
<button type="submit" form="form_config" value="Submit">Confirma</button>


</div>
</body></html>
