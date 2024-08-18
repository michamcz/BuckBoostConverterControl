let ile = 10;
const przycisk1 = document.getElementById("przycisk1");
const przycisk2 = document.getElementById("przycisk2");
const wartosc = document.getElementById("wartosc");
const DVal = document.querySelector("#DVal");
const inputD = document.querySelector("#inputD");
const startButtonHandle = document.querySelector("#startButton");
const stopButtonHandle = document.querySelector("#stopButton");
const configButtonHandle = document.querySelector("#sendConfigButton");
const deadTime = document.querySelector("#givenDeadTime");
const currentLimitHandle = document.querySelector("#givenCurrentLimit");

const DCycleButtonHandle = document.querySelector("#pwmModeButton");
const PIDButtonHandle = document.querySelector("#currentModeButton");
const inputGivenCurrent = document.querySelector("#givenCurrent");
const inputGivenTs = document.querySelector("#givenTs");
const inputGivenTr = document.querySelector("#givenTr");
const inputGivenKp = document.querySelector("#givenKp");

const actualCurrentVal = document.querySelector("#actualCurrentVal");
const DCurrentVal = document.querySelector("#DCurrentVal");
const Q1State = document.querySelector("#Q1State");
const Q2State = document.querySelector("#Q2State");
const Q3State = document.querySelector("#Q3State");
const Q4State = document.querySelector("#Q4State");
const buckState = document.querySelector("#buckState");
const boostState = document.querySelector("#boostState");
const currentLimitP = document.querySelector("#currentLimitP");
const resetButtonHandle = document.querySelector("#resetButton");

//let firstUpdate = true;
let statusOnOff = false;
let statusBuck = true;
let mode = 1;
let D = 0;
let IGiv = 0;
let IAct = 0;
let emergencyStop = false;

let myInterval;
let resetInterval;
//const ipAddress = "192.168.1.140";
const ipAddress = window.location.hostname;

inputD.addEventListener("input", (event) => {
  DVal.textContent = event.target.value;
  D = event.target.value;
});

window.onload = () => {
  synchronizeData();
};

function PIDModeButton() {
  mode = 0;
  inputD.setAttribute("disabled", "disabled");
  inputGivenCurrent.removeAttribute("disabled");
  inputGivenTs.removeAttribute("disabled");
  inputGivenTr.removeAttribute("disabled");
  inputGivenKp.removeAttribute("disabled");
  PIDButtonHandle.classList.add("modeButtonActive");
  DCycleButtonHandle.classList.remove("modeButtonActive");
}

function DCycleButton() {
  mode = 1;
  inputD.removeAttribute("disabled");
  inputGivenCurrent.setAttribute("disabled", "disabled");
  inputGivenTs.setAttribute("disabled", "disabled");
  inputGivenTr.setAttribute("disabled", "disabled");
  inputGivenKp.setAttribute("disabled", "disabled");
  DCycleButtonHandle.classList.add("modeButtonActive");
  PIDButtonHandle.classList.remove("modeButtonActive");
}

async function resetButton() {
  const response = await fetch(`http://${ipAddress}/RESET`);
  if (response.ok) {
    emergencyStop = false;
    clearInterval(resetInterval);
    synchronizeData();
  }
}

async function synchronizeData() {
  const response = await fetch(`http://${ipAddress}/GETSTATE`);
  if (response.ok) {
    const data = await response.json();
    statusOnOff = data.status;
    statusBuck = data.statusBuck;
    D = data.D;
    IGiv = data.IGiv;
    IAct = data.IAct;
    emergencyStop = data.emergencyStop;
    if (emergencyStop == true) {
      clearInterval(myInterval);
    }
    updateStatus();
  }
}

async function applyButton() {
  const response = await fetch(
    `http://${ipAddress}/APPLYD?val=${DVal.textContent}`
  );
  if (response.ok) {
    synchronizeData();
  }
}

async function startButton() {
  fetch(`http://${ipAddress}/SETD?val=${inputD.value}`);
  fetch(
    `http://${ipAddress}/SETDATA?DeadTime=${deadTime.value * 10}&I=${
      inputGivenCurrent.value
    }&CurrentLimit=${currentLimitHandle.value}`
  );

  const response = await fetch(`http://${ipAddress}/START?mode=${mode}`);
  clearInterval(myInterval);
  if (response.ok) {
    synchronizeData();
    myInterval = setInterval(synchronizeData, 2000);
  }
}

async function stopButton() {
  const response = await fetch(`http://${ipAddress}/STOP`);
  if (response.ok) {
    clearInterval(myInterval);
    synchronizeData();
  }
}

function updateStatus() {
  if (emergencyStop == true) {
    currentLimitP.classList.add("currentLimitEnable");
    resetButtonHandle.removeAttribute("disabled");
    startButtonHandle.setAttribute("disabled", "disabled");
    resetInterval = setInterval(() => {
      resetButtonHandle.classList.toggle("resetButtonBlink");
    }, 800);
  } else {
    currentLimitP.classList.remove("currentLimitEnable");
    resetButtonHandle.setAttribute("disabled", "disabled");
  }
  if (statusOnOff == true) {
    stopButtonHandle.removeAttribute("disabled");
    startButtonHandle.setAttribute("disabled", "disabled");
    DCycleButtonHandle.setAttribute("disabled", "disabled");
    PIDButtonHandle.setAttribute("disabled", "disabled");
    deadTime.setAttribute("disabled", "disabled");
    currentLimitHandle.setAttribute("disabled", "disabled");
    if (statusBuck == true) {
      buckState.innerText = "BUCK: ON";
      buckState.classList.add("buckBoostStateActive");
      boostState.classList.remove("buckBoostStateActive");
      DCurrentVal.innerText = `D = ${D}%`;
      actualCurrentVal.innerText = `I = ${IAct.toFixed(2)}A`;
      Q1State.innerText = `Q1 = PWM`;
      Q2State.innerText = `Q2 = PWM`;
      Q3State.innerText = `Q3 = HIGH`;
      Q4State.innerText = `Q4 = LOW`;
    } else if (statusBuck == false) {
      boostState.innerText = "BOOST: ON";
      boostState.classList.add("buckBoostStateActive");
      buckState.classList.remove("buckBoostStateActive");
      DCurrentVal.innerText = `D = ${D - 100}%`;
      actualCurrentVal.innerText = `I = ${IAct.toFixed(2)}A`;
      Q1State.innerText = `Q1 = HIGH`;
      Q2State.innerText = `Q2 = LOW`;
      Q3State.innerText = `Q3 = PWM`;
      Q4State.innerText = `Q4 = PWM`;
    }
  } else if (statusOnOff == false) {
    if (emergencyStop == false) {
      startButtonHandle.removeAttribute("disabled");
    }
    stopButtonHandle.setAttribute("disabled", "disabled");
    DCycleButtonHandle.removeAttribute("disabled");
    PIDButtonHandle.removeAttribute("disabled");
    deadTime.removeAttribute("disabled");
    currentLimitHandle.removeAttribute("disabled");
    buckState.innerText = "BUCK: OFF";
    buckState.classList.remove("buckBoostStateActive");
    boostState.innerText = "BOOST: OFF";
    boostState.classList.remove("buckBoostStateActive");
    actualCurrentVal.innerText = `I = ---`;
    DCurrentVal.innerText = `D = ---`;
    Q1State.innerText = `Q1 = ---`;
    Q2State.innerText = `Q2 = ---`;
    Q3State.innerText = `Q3 = ---`;
    Q4State.innerText = `Q4 = ---`;
  }
}
