// Footer of every page: firmware version from /version.json
(function() {
  const footer = document.getElementById("version");
  fetch("version.json")
    .then(response => response.json())
    .then(info => {
      footer.textContent = "BananaCoffee " + info.Version;
      footer.title = "ESP-IDF " + info.IdfVersion + ", built " + info.Built;
    })
    .catch(() => {}); // keep the plain name
})();
