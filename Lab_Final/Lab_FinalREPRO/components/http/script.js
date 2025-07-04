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
