window.addEventListener('load',()=>{document.fonts.ready.then(()=>{const bad=[];
document.querySelectorAll('.mon, .pg .in').forEach((e,i)=>{if(e.scrollHeight>e.clientHeight+1||e.scrollWidth>e.clientWidth+1){const pg=e.closest('.pg');const idx=[...document.querySelectorAll('.pg')].indexOf(pg)+1;const no=e.querySelector('.no');bad.push(idx+':'+(no?no.textContent:'in'))}});
document.querySelectorAll('.mon .r').forEach(e=>{const m=e.closest('.mon');if(e.getBoundingClientRect().bottom>m.getBoundingClientRect().bottom+0.5){const no=m.querySelector('.no');bad.push('R:'+(no?no.textContent:''))}});
document.body.setAttribute('data-bad',bad.join(' '));document.body.setAttribute('data-done','1')})});
