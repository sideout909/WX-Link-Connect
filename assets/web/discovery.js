/* Local-only station discovery. Native RF input stays on the device. */
(() => {
  'use strict';
  const $ = id => document.getElementById(id);
  const form = $('settings'), list = $('discovery-candidates'), message = $('discovery-message');
  let pending = false, active = false, selected = 0, session = 0;
  let reconciling=false,refreshPromise=null;
  const dismissed=new Set();
  const text = (tag, value) => {const el=document.createElement(tag);el.textContent=value;return el;};
  async function request(path, mutation=false) {
    const controller=new AbortController(), timer=setTimeout(()=>controller.abort(),12000);
    try {
      const options={cache:'no-store',signal:controller.signal};
      if(mutation){
        const configResponse=await fetch('/api/config',options);
        if(!configResponse.ok)throw Error('Sign in again, then retry.');
        const config=await configResponse.json();
        options.method='POST';options.headers={'X-CSRF-Token':config.csrf};
      }
      const response=await fetch(path,options);
      let result;try{result=await response.json();}catch{throw Error('Cannot read detection status. Try again.');}
      if(!response.ok)throw Error(result.error||'Detection request failed.');
      return result;
    } finally {clearTimeout(timer);}
  }
  function clearSelection() {
    selected=0;form.elements.discovery_session.value='0';form.elements.discovery_candidate.value='0';
    if(window.wxConfig){form.elements.station_id.value=window.wxConfig.station_id;form.elements.channel.value=window.wxConfig.channel;}
  }
  function configure() {
    const configured=window.wxConfig?.station_confirmed!==false;
    $('discovery-binding').textContent=configured?'Your saved station stays connected while you look for a device.':'Find and confirm your station to start recording and sharing its readings.';
  }
  function render(state) {
    active=!!state.active;
    if(session && state.session!==session){clearSelection();dismissed.clear();}
    session=state.session||0;
    $('discovery-start').disabled=active||pending;
    $('discovery-start').textContent=state.expired?'Scan again':'Find my station';
    $('discovery-cancel').hidden=!active;
    $('discovery-progress').hidden=!session;
    $('discovery-progress').value=state.elapsed_s||0;
    if(!selected)message.textContent=active?'Listening… '+Math.floor((state.elapsed_s||0)/60)+':'+String((state.elapsed_s||0)%60).padStart(2,'0')+' / 5:00. Keep your outdoor sensor powered on.':state.expired?'Scan complete. If your station is missing, scan again or use Advanced manual setup.':'Ready to find your station.';
    if(!state.ready)message.textContent='Receiver unavailable. Enable reception, save, then check wiring and power.';
    list.replaceChildren();
    for(const candidate of state.candidates||[]) {
      if(dismissed.has(candidate.token))continue;
      const card=text('div','');card.className='card';
      card.append(text('h3',candidate.name));
      const readings=[];
      if(Number.isFinite(candidate.temperature_F))readings.push(candidate.temperature_F.toFixed(1)+' °F');
      if(Number.isFinite(candidate.humidity))readings.push(candidate.humidity+'% humidity');
      if(Number.isFinite(candidate.wind_mph))readings.push(candidate.wind_mph.toFixed(1)+' mph wind');
      card.append(text('p',readings.join(' · ')||'Waiting for more measurements…'));
      const details=document.createElement('details');details.append(text('summary','Detection details'));
      details.append(text('p','Sensor '+candidate.id+' · channel '+candidate.channel+' · '+candidate.sightings+' independent sightings · last heard '+candidate.age_s+'s ago · '+state.profile));
      card.append(details);
      if(!candidate.supported)card.append(text('p','Detected successfully. Integration for this station family is not available in this firmware yet.'));
      else if(candidate.selectable){
        card.append(text('p',selected===candidate.token?'Selected. Save settings to use this station.':'Is this your weather station?'));
        const use=text('button',selected===candidate.token?'Selected':'Use this station');use.type='button';use.className='primary';use.disabled=selected===candidate.token;
        use.onclick=()=>{
          selected=candidate.token;form.elements.discovery_session.value=String(state.session);form.elements.discovery_candidate.value=String(candidate.token);
          form.elements.station_id.value=String(candidate.id);form.elements.channel.value=candidate.channel;
          form.elements.manual_binding.checked=false;form.elements.radio_enabled.checked=true;
          message.textContent=candidate.name+' selected. Save settings to confirm and restart. Your current station remains active until then.';
          render(state);
        };card.append(use);
        const keep=text('button','Keep looking');keep.type='button';keep.style.marginLeft='8px';
        keep.onclick=()=>{if(selected===candidate.token)clearSelection();dismissed.add(candidate.token);render(state);};
        card.append(keep);
      }else card.append(text('p',candidate.age_s>=120?'Signal is no longer current. Keep listening or scan again.':candidate.suggested?'Waiting for a complete set of Atlas measurements…':'Confirming repeated transmissions…'));
      list.append(card);
    }
    if(state.overflow)list.append(text('p','More devices were heard than can be listed. Move nearer your station and scan again.'));
  }
  function refresh() {
    // Mobile connections can lose a response after the device accepted Start.
    // Serialize status reads and reconcile that scan; never retry the POST
    // automatically or replace a useful running state with its earlier error.
    if(refreshPromise)return refreshPromise;
    refreshPromise=(async()=>{
      try {render(await request('/api/discovery'));reconciling=false;return true;}
      catch(error){message.textContent=(active||reconciling?'Connection interrupted; checking scan status again. ':'Detection status unavailable: ')+error.message;return false;}
      finally {refreshPromise=null;}
    })();
    return refreshPromise;
  }
  $('discovery-start').onclick=async()=>{
    if(pending)return;pending=true;$('discovery-start').disabled=true;
    try {
      if(refreshPromise)await refreshPromise;
      await request('/api/discovery/start',true);
      active=true;clearSelection();dismissed.clear();$('discovery-cancel').hidden=false;
      await refresh();
    }
    catch(error){
      reconciling=true;
      const known=await refresh();
      if(!active&&known)message.textContent=error.message;
    }
    finally {pending=false;$('discovery-start').disabled=active||reconciling;}
  };
  $('discovery-cancel').onclick=async()=>{
    if(pending)return;pending=true;
    try{if(refreshPromise)await refreshPromise;await request('/api/discovery/cancel',true);clearSelection();active=false;await refresh();message.textContent='Scan cancelled. Your saved station is unchanged.';}
    catch(error){message.textContent=error.message;}
    finally{pending=false;$('discovery-start').disabled=active;}
  };
  for(const name of ['station_id','channel','manual_binding'])form.elements[name].addEventListener('change',()=>{
    if(selected){selected=0;form.elements.discovery_session.value='0';form.elements.discovery_candidate.value='0';message.textContent='Automatic selection cleared. Use manual binding or find your station again.';}
  });
  document.addEventListener('wx-config',configure);
  setInterval(()=>{if((active||reconciling)&&!pending&&!document.hidden&&$('station').classList.contains('active'))refresh();},2000);
  if(window.wxConfig)configure();
  window.WXDiscovery={configure,refresh};
})();
