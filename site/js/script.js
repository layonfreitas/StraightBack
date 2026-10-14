function voltar() {
  window.location.href = "index.html"; 
}

const ctx = document.getElementById('postureChart').getContext('2d');

new Chart(ctx, {
  type: 'bar',
  data: {
    labels: ['Cervical','Torácica','Lombar'],
    datasets: [
      { label: 'Em pé (min)', data: [16, 2, 16], backgroundColor: '#667eea' },
      { label: 'Sentado (min)', data: [36, 3, 36], backgroundColor: '#764ba2' }
    ]
  },
  options: {
    responsive: true,
    maintainAspectRatio: false, // mantém proporção flexível
    scales: { 
      x: { stacked: true }, 
      y: { stacked: true, beginAtZero: true } 
    },
    plugins: { 
      legend: { position: 'top' } 
    }
  }
});