/* Weather Link browser-only station picker. External services receive only
 * the ZIP/point or visible map area. No device credentials are forwarded. */
(() => {
  'use strict';
  const form=document.querySelector('#settings'), shell=document.querySelector('#map-shell');
  const message=document.querySelector('#map-message'), selected=document.querySelector('#map-selection');
  const elevation=document.querySelector('#map-elevation'), use=document.querySelector('#map-use');
  let map,marker,leafletPromise,candidate=null,revision=0,searchRevision=0,elevationRequest,zipRequest,appliedPoint;
  const coordinate=(v,min,max)=>v!==null&&String(v).trim()!==''&&Number.isFinite(Number(v))&&Number(v)>=min&&Number(v)<=max;
  const validPoint=(lat,lon)=>coordinate(lat,-90,90)&&coordinate(lon,-180,180);
  function syncRequired(){
    for(const name of ['latitude','longitude','elevation_m'])form.elements[name].required=form.elements.location_set.checked;
  }
  form.elements.location_set.addEventListener('change',syncRequired);
  // Config loading assigns checkboxes programmatically; synchronize after it.
  document.addEventListener('wx-config',syncRequired);
  syncRequired();
  function clearCandidate(){
    revision++;elevationRequest?.abort();candidate=null;use.disabled=true;
    selected.textContent='Tap the map or use its center to drop the station pin.';
    elevation.textContent='Ground elevation will be looked up for your selected point.';
    if(marker){map.removeLayer(marker);marker=null;}
  }
  async function remoteJSON(url,controller){
    const timer=setTimeout(()=>controller.abort(),12000);
    try{
      const response=await fetch(url,{mode:'cors',credentials:'omit',referrerPolicy:'origin',signal:controller.signal});
      if(!response.ok)throw Error('Service unavailable ('+response.status+').');
      return await response.json();
    }finally{clearTimeout(timer);}
  }
  function leaflet(){
    if(window.L)return Promise.resolve();
    if(leafletPromise)return leafletPromise;
    leafletPromise=new Promise((resolve,reject)=>{
      const css=document.createElement('link');css.rel='stylesheet';css.href='/assets/leaflet-1.9.4.css';document.head.append(css);
      const script=document.createElement('script');script.src='/assets/leaflet-1.9.4.js';
      const timer=setTimeout(()=>{script.remove();leafletPromise=null;reject(Error('Map controls could not load. Manual entry still works.'));},12000);
      script.onload=()=>{clearTimeout(timer);resolve();};
      script.onerror=()=>{clearTimeout(timer);script.remove();leafletPromise=null;reject(Error('Map controls could not load. Manual entry still works.'));};
      document.head.append(script);
    });
    return leafletPromise;
  }
  function cancelZip(){searchRevision++;zipRequest?.abort();document.querySelector('#zip-search').disabled=false;}
  async function openMap(center,zoom,token=searchRevision){
    await leaflet();if(token!==searchRevision)return null;shell.hidden=false;
    if(!map){
      map=L.map('location-map',{worldCopyJump:true,zoomControl:true});
      L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png',{
        maxZoom:19,keepBuffer:0,updateWhenIdle:true,referrerPolicy:'origin',
        attribution:'&copy; <a href="https://www.openstreetmap.org/copyright" target="_blank" rel="noopener">OpenStreetMap</a> contributors'
      }).on('tileerror',()=>{message.textContent='Map tiles unavailable. Check browser internet access or use manual coordinates.';}).addTo(map);
      map.on('click',e=>pick(e.latlng.lat,e.latlng.lng));
      map.getContainer().setAttribute('aria-label','Station location map. Pan with arrow keys, zoom with plus or minus, then use Drop pin at map center.');
      map.getContainer().setAttribute('tabindex','0');
    }
    const lat=form.elements.latitude.value,lon=form.elements.longitude.value;
    if(!center){
      const configured=form.elements.location_set.checked&&validPoint(lat,lon);
      center=configured?[Number(lat),Number(lon)]:[39.8,-98.6];zoom=configured?15:4;
    }
    clearCandidate();map.invalidateSize();map.setView(center,zoom,{animate:false});
    return map;
  }
  async function pick(lat,lon){
    // Leaflet can return wrapped longitudes after panning across the dateline.
    lon=((lon+180)%360+360)%360-180;
    if(!validPoint(lat,lon))return;
    const point={lat:Number(lat.toFixed(5)),lon:Number(lon.toFixed(5)),elevation:null};
    const token=++revision;candidate=point;use.disabled=true;elevationRequest?.abort();
    if(!marker){
      marker=L.marker([point.lat,point.lon],{draggable:true,title:'Drag station pin',
        icon:L.divIcon({className:'station-pin',html:'<span></span>',iconSize:[28,36],iconAnchor:[14,36]})}).addTo(map);
      marker.on('dragend',()=>{const p=marker.getLatLng();pick(p.lat,p.lng);});
    }else marker.setLatLng([point.lat,point.lon]);
    selected.textContent='Selected point: '+point.lat.toFixed(5)+', '+point.lon.toFixed(5)+'. Not applied yet.';
    elevation.textContent='Looking up ground elevation from USGS…';
    message.textContent='Review the pin, then choose Use this location. Save and restart applies it to the device.';
    elevationRequest=new AbortController();
    const query=new URLSearchParams({x:String(point.lon),y:String(point.lat),units:'Meters',wkid:'4326',includeDate:'false'});
    try{
      const result=await remoteJSON('https://epqs.nationalmap.gov/v1/json?'+query,elevationRequest);
      if(token!==revision)return;
      if(!coordinate(result.value,-500,9000))throw Error('No usable elevation for this point.');
      point.elevation=Math.round(Number(result.value)*10)/10;
      elevation.textContent='USGS ground estimate: '+point.elevation.toFixed(1)+' m / '+Math.round(point.elevation*3.28084)+' ft. You can edit it after applying.';
    }catch(e){
      if(token!==revision)return;
      elevation.textContent='Elevation unavailable for this point. Use this location, then enter ground elevation manually before saving.';
    }finally{if(token===revision)use.disabled=false;}
  }
  document.querySelector('#map-open').onclick=async()=>{
    try{cancelZip();message.textContent='Opening map…';if(!await openMap())return;message.textContent='Pan and zoom, then drop or drag the pin to your station.';}
    catch(e){message.textContent=e.message;}
  };
  document.querySelector('#zip-search').onclick=async()=>{
    const zip=document.querySelector('#zip').value.trim();
    if(!/^\d{5}(?:-\d{4})?$/.test(zip)){message.textContent='Enter a US ZIP code: five digits, optionally ZIP+4.';return;}
    const token=++searchRevision;const button=document.querySelector('#zip-search');button.disabled=true;zipRequest?.abort();zipRequest=new AbortController();
    message.textContent='Finding ZIP area…';
    try{
      const result=await remoteJSON('https://api.zippopotam.us/us/'+zip.slice(0,5),zipRequest);
      if(token!==searchRevision)return;
      const place=result.places?.find(p=>validPoint(p.latitude,p.longitude));
      if(!place)throw Error('No location found for that ZIP code.');
      if(!await openMap([Number(place.latitude),Number(place.longitude)],12,token))return;
      message.textContent=(place['place name']||'ZIP area')+(place['state abbreviation']?', '+place['state abbreviation']:'')+' · ZIP '+zip.slice(0,5)+'. Approximate area only: drop the pin at your station.';
    }catch(e){if(token!==searchRevision)return;message.textContent='ZIP lookup failed. Check the ZIP and browser internet access, or enter coordinates manually.';}
    finally{if(token===searchRevision)button.disabled=false;}
  };
  document.querySelector('#zip').addEventListener('keydown',e=>{if(e.key==='Enter'){e.preventDefault();document.querySelector('#zip-search').click();}});
  document.querySelector('#map-center').onclick=()=>{if(map){const p=map.getCenter();pick(p.lat,p.lng);}};
  document.querySelector('#map-close').onclick=()=>{cancelZip();clearCandidate();shell.hidden=true;message.textContent='Map closed. Manual coordinates remain available.';};
  use.onclick=()=>{
    if(!candidate||use.disabled)return;
    form.elements.latitude.value=candidate.lat.toFixed(5);form.elements.longitude.value=candidate.lon.toFixed(5);
    form.elements.elevation_m.value=candidate.elevation===null?'':candidate.elevation.toFixed(1);
    appliedPoint={...candidate};form.elements.location_set.checked=true;syncRequired();
    selected.textContent='Point copied to the form. Review coordinates, elevation and timezone, then Save and restart.';
    if(candidate.elevation===null)form.elements.elevation_m.focus();
  };
  for(const name of ['latitude','longitude'])form.elements[name].addEventListener('input',()=>{
    if(appliedPoint&&(Number(form.elements.latitude.value)!==appliedPoint.lat||Number(form.elements.longitude.value)!==appliedPoint.lon)){
      if(appliedPoint.elevation!==null&&Number(form.elements.elevation_m.value)===appliedPoint.elevation)form.elements.elevation_m.value='';
      appliedPoint=null;message.textContent='Coordinates changed. Enter ground elevation for the new point, or select it on the map.';
    }
  });
  window.WXLocation={resize(){if(!shell.hidden)requestAnimationFrame(()=>map?.invalidateSize());}};
})();
