function doPost(e) {
  var sheet = SpreadsheetApp.getActiveSpreadsheet().getActiveSheet();
  var data = JSON.parse(e.postData.contents);

  var formattedDate = Utilities.formatDate(new Date(), "GMT+7", "dd MMM yyyy");
  var formattedTime = Utilities.formatDate(new Date(), "GMT+7", "HH:mm:ss");

  // Masukkan data ke sheet
  sheet.appendRow([
    formattedDate,
    formattedTime,
    data.rssi,
    data.snr,
    data.ap,
    data.distance,
    data.frequency,
    data.tx_power,
    data.antenna_gain,
    data.status
  ]);

  var lastRow = sheet.getLastRow();
  var range = sheet.getRange(lastRow, 1, 1, 10);

  var warna = "#FFFFFF"; // default putih

  switch (data.status) {
    case "Koneksi Sangat Baik":
      warna = "#03fc03";
      break;
    case "Koneksi Baik":
      warna = "#00a600";
      break;
    case "Koneksi Cukup":
      warna = "#fcdb03";
      break;
    case "Koneksi Buruk":
      warna = "#baa200";
      break;
    case "Koneksi Terputus":
      warna = "#ff0000";
      break;
  }

  range.setBackground(warna);

  return ContentService.createTextOutput("Success").setMimeType(ContentService.MimeType.TEXT);
}

