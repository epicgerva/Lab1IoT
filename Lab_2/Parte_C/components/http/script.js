document.addEventListener("DOMContentLoaded", function () {
  const timeElement = document.getElementById("currentTime");
  if (timeElement) {
    function updateTime() {
      const now = new Date();
      const hours = String(now.getHours()).padStart(2, "0");
      const minutes = String(now.getMinutes()).padStart(2, "0");
      const seconds = String(now.getSeconds()).padStart(2, "0");
      const timeString = `${hours}:${minutes}:${seconds}`;
      timeElement.textContent = timeString;
    }
    updateTime();
    setInterval(updateTime, 1000);
  } else {
    console.error('Elemento "currentTime" no encontrado');
  }
});
