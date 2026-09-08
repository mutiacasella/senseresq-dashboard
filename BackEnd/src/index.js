const express = require("express");
const cors = require("cors");
require("dotenv").config();
require("./config/database.js");

const deviceRoutes = require("./modules/device/device.route");
const volunteerRoutes = require("./modules/volunteer/volunteer.route");

const app = express();
const PORT = process.env.PORT || 7777;

app.use(cors());
app.use(express.json());

// Routes
app.use(deviceRoutes);
app.use(volunteerRoutes);

app.get("/", (req, res) => {
    res.send("SenseResQ Backend is running!");
});

app.listen(PORT, () => {
    console.log(`Server is running on port ${PORT}`);
});