document.addEventListener("DOMContentLoaded", function () {
  function sendCommand(cmd) {
    const params = new URLSearchParams();
    params.append("comando", cmd);
    fetch("/comando", {
      method: "POST",
      headers: {
        "Content-Type": "application/x-www-form-urlencoded",
      },
      body: params.toString(),
    });
  }

  function loadConfig() {
    fetch("/api/wifi-config")
      .then((response) => response.json())
      .then((data) => {
        // Handle WiFi configuration
        if (data.wifi && data.wifi.configured) {
          console.log("Current WiFi config:", data.wifi);
          
          if (data.wifi.mode === "STA") {
            document.getElementById("ssid").value = data.wifi.ssid;
            document.getElementById("password").value = data.wifi.password;
            document.getElementById(
              "ssid"
            ).placeholder = `Current: ${data.wifi.ssid}`;
            document.getElementById(
              "password"
            ).placeholder = `Current: ${data.wifi.password}`;
          } else if (data.wifi.mode === "AP") {
            document.getElementById("ssid2").value = data.wifi.ssid;
            document.getElementById("password2").value = data.wifi.password;
            document.getElementById(
              "ssid2"
            ).placeholder = `Current: ${data.wifi.ssid}`;
            document.getElementById(
              "password2"
            ).placeholder = `Current: ${data.wifi.password}`;
          }
          
          updateWifiModeIndicator(data.wifi.mode);
        } else {
          console.log("No WiFi config found, using defaults");
          updateWifiModeIndicator("AP");
        }
        
        // Handle MQTT configuration
        if (data.mqtt && data.mqtt.configured) {
          console.log("Current MQTT config:", data.mqtt);
          
          document.getElementById("broker").value = data.mqtt.broker;
          document.getElementById("puerto").value = data.mqtt.puerto;
          document.getElementById("topic_evento").value = data.mqtt.topic_evento;
          document.getElementById("topic_buffer").value = data.mqtt.topic_buffer;
          
          document.getElementById(
            "broker"
          ).placeholder = `Current: ${data.mqtt.broker}`;
          document.getElementById(
            "puerto"
          ).placeholder = `Current: ${data.mqtt.puerto}`;
          document.getElementById(
            "topic_evento"
          ).placeholder = `Current: ${data.mqtt.topic_evento}`;
          document.getElementById(
            "topic_buffer"
          ).placeholder = `Current: ${data.mqtt.topic_buffer}`;
        } else {
          console.log("No MQTT config found, using defaults");
        }
      })
      .catch((error) => {
        console.error("Error loading config:", error);
        updateWifiModeIndicator("AP");
      });
  }

  function updateWifiModeIndicator(mode) {
    let staIndicator = document.getElementById("sta-mode-indicator");
    let apIndicator = document.getElementById("ap-mode-indicator");

    if (!staIndicator) {
      staIndicator = document.createElement("div");
      staIndicator.id = "sta-mode-indicator";
      staIndicator.className = "mode-indicator";
      staIndicator.innerHTML = "En modo estaci&oacute;n";
      document
        .querySelector("form[action='/sta']")
        .parentElement.insertBefore(
          staIndicator,
          document.querySelector("form[action='/sta']")
        );
    }

    if (!apIndicator) {
      apIndicator = document.createElement("div");
      apIndicator.id = "ap-mode-indicator";
      apIndicator.className = "mode-indicator";
      apIndicator.innerHTML = "En modo punto de acceso";
      document
        .querySelector("form[action='/ap']")
        .parentElement.insertBefore(
          apIndicator,
          document.querySelector("form[action='/ap']")
        );
    }

    if (mode === "STA") {
      staIndicator.style.display = "block";
      apIndicator.style.display = "none";
    } else {
      staIndicator.style.display = "none";
      apIndicator.style.display = "block";
    }
  }

  // Load all configuration on page load
  loadConfig();

  document.getElementById("playButton").addEventListener("click", function () {
    sendCommand("play");
  });

  document.getElementById("pauseButton").addEventListener("click", function () {
    sendCommand("pause");
  });

  document.getElementById("stopButton").addEventListener("click", function () {
    sendCommand("stop");
  });

  document.getElementById("nextButton").addEventListener("click", function () {
    sendCommand("next");
  });

  document
    .getElementById("previousButton")
    .addEventListener("click", function () {
      sendCommand("prev");
    });

  document
    .getElementById("volumeUpButton")
    .addEventListener("click", function () {
      sendCommand("volup");
    });

  document
    .getElementById("volumeDownButton")
    .addEventListener("click", function () {
      sendCommand("voldown");
    });
});
