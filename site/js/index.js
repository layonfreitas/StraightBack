const firebaseConfig = {
  apiKey: "AIzaSyDHLmV0k7kARWv-q2PvenSfq4IRc5fl9BQ",
  authDomain: "projete2309-82195.firebaseapp.com",
  databaseURL: "https://projete2309-82195-default-rtdb.firebaseio.com",
  projectId: "projete2309-82195",
  storageBucket: "projete2309-82195.firebasestorage.app",
  messagingSenderId: "333908066296",
  appId: "1:333908066296:web:5337552b44485437ba600d",
  measurementId: "G-ERRB9ENE5Y"
};
firebase.initializeApp(firebaseConfig);
const auth = firebase.auth();
const DATA_ENDPOINT = 'https://teste-maiq.onrender.com/';

let postureData = null;
let chart = null;

// -------------------- VARIÁVEIS PARA O RELÓGIO --------------------
let intermediario = JSON.parse(localStorage.getItem("intermediario")) || { 
  cervical: 0, 
  toracica: 0, 
  lombar: 0 
};

let ultimoReset = Number(localStorage.getItem("ultimoReset")) || Date.now();
const INTERVALO_RESET = 300000; // 1 minuto (para teste)


function checkDayChange() {
  const agora = Date.now();
  if (agora - ultimoReset >= INTERVALO_RESET && postureData) {
    intermediario.cervical = postureData.cervical || 0;
    intermediario.toracica = postureData.toracica || 0;
    intermediario.lombar = postureData.lombar || 0;
    ultimoReset = agora;

    // salva no navegador
    localStorage.setItem("intermediario", JSON.stringify(intermediario));
    localStorage.setItem("ultimoReset", ultimoReset);

    console.log("RESET feito, novos valores intermediários:", intermediario);
  }
}



auth.onAuthStateChanged(user => {
  if(user){
    document.getElementById("navbar").style.display = "flex";
    showView('dashboard');
    loadPostureData();
  } else {
    document.getElementById("navbar").style.display = "none";
    showView('login');
  }
});

document.getElementById('loginForm').addEventListener('submit', async ev=>{
  ev.preventDefault();
  const email = document.getElementById('email').value.trim();
  const password = document.getElementById('password').value;
  const err = document.getElementById('loginError');
  err.textContent = '';
  try { await auth.signInWithEmailAndPassword(email,password); } 
  catch(e){ err.textContent = 'Erro no login: '+(e.message||e.code||e); }
});

function logout(){ auth.signOut(); }

function showView(viewName){
  document.querySelectorAll('.view').forEach(v=>v.classList.remove('active'));
  document.getElementById(viewName+'View').classList.add('active');
  document.querySelectorAll('.nav-btn').forEach(btn=>{
    const v = btn.getAttribute('data-view');
    btn.classList.toggle('active', v===viewName);
  });
}
function switchView(v){ showView(v); }

async function loadPostureData(){
  document.getElementById('loading').style.display = 'block';
  document.getElementById('dashboardData').style.display = 'none';
  try{
    const res = await fetch(DATA_ENDPOINT,{cache:'no-store'});
    if(!res.ok) throw new Error(res.statusText);
    const json = await res.json();
    postureData = json||{};

    // garante números
    postureData.cervical = Number(postureData.cervical||0);
    postureData.toracica = Number(postureData.toracica||postureData.torax||0);
    postureData.lombar = Number(postureData.lombar||0);

    checkDayChange(); // <-- verifica se virou o dia

    updateDashboard();
    updateParameters();
    createChart();

    document.getElementById('loading').style.display = 'none';
    document.getElementById('dashboardData').style.display = 'block';
  }catch(err){ document.getElementById('loading').textContent = 'Erro ao carregar dados'; }
}

function updateDashboard(){
  if(!postureData) return;

  // aplica correção: valorESP - intermediario
  const cervicalMinutes = Math.round(((postureData.cervical||0) - intermediario.cervical)/60000);
  const toracicaMinutes = Math.round(((postureData.toracica||0) - intermediario.toracica)/60000);
  const lombarMinutes = Math.round(((postureData.lombar||0) - intermediario.lombar)/60000);

  document.getElementById('cervicalTime').textContent = cervicalMinutes+' min';
  document.getElementById('toracicaTime').textContent = toracicaMinutes+' min';
  document.getElementById('lombarTime').textContent = lombarMinutes+' min';
}

function updateParameters(){
  if(!postureData) return;
  document.getElementById('paramCervicalMax').textContent = postureData.cervicalmax?.toFixed?.(2) ?? '--';
  document.getElementById('paramCervicalMin').textContent = postureData.cervicalmin?.toFixed?.(2) ?? '--';
  document.getElementById('paramToraxMax').textContent = postureData.toraxmax?.toFixed?.(2) ?? '--';
  document.getElementById('paramToraxMin').textContent = postureData.toraxmin?.toFixed?.(2) ?? '--';
  document.getElementById('paramLombarMax').textContent = postureData.lombarmax?.toFixed?.(2) ?? '--';
  document.getElementById('paramLombarMin').textContent = postureData.lombarmin?.toFixed?.(2) ?? '--';
  if(postureData.sentado === true){
    document.getElementById('paramSentado').textContent = 'Sentado';
  } else {
    document.getElementById('paramSentado').textContent = 'Em pé';
  }
}

function createChart(){
  if(!postureData) return;
  const ctx = document.getElementById('postureChart').getContext('2d');

  // aplica correção aqui também
  const cervicalMinutes = Math.round(((postureData.cervical||0) - intermediario.cervical)/60000);
  const toracicaMinutes = Math.round(((postureData.toracica||0) - intermediario.toracica)/60000);
  const lombarMinutes = Math.round(((postureData.lombar||0) - intermediario.lombar)/60000);

  const standingData = postureData.sentado ? 
    [Math.round(cervicalMinutes*0.3), Math.round(toracicaMinutes*0.3), Math.round(lombarMinutes*0.3)] :
    [cervicalMinutes, toracicaMinutes, lombarMinutes];

  const sittingData = postureData.sentado ? 
    [Math.round(cervicalMinutes*0.7), Math.round(toracicaMinutes*0.7), Math.round(lombarMinutes*0.7)] :
    [Math.round(cervicalMinutes*0.2), Math.round(toracicaMinutes*0.2), Math.round(lombarMinutes*0.2)];

  if(chart) chart.destroy();
  chart = new Chart(ctx,{
    type:'bar',
    data:{
      labels:['Cervical','Torácica','Lombar'],
      datasets:[
        {label:'Em pé (min)',data:standingData,backgroundColor:'#667eea'},
        {label:'Sentado (min)',data:sittingData,backgroundColor:'#764ba2'}
      ]
    },
    options:{
      responsive:true,
      scales:{ x:{stacked:true}, y:{stacked:true, beginAtZero:true} },
      plugins:{
        legend:{ position:'top' },
        tooltip:{
          mode:'index',
          intersect:false,
          callbacks:{ footer:(items)=>{ 
            let total=0; items.forEach(it=>{ total+=it.parsed.y??0 }); 
            return 'Total: '+total+' min'; 
          } }
        }
      }
    }
  });
}

setInterval(()=>{ 
  if(auth.currentUser) loadPostureData(); 
}, 30000);

const chartCanvas = document.getElementById('postureChart');
chartCanvas.addEventListener('click', () => {
  window.open('detalhes.html');
});
