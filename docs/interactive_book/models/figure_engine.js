(function(root){
  'use strict';
  const kinds=['sequence','state','ring','slots','queue','bytes','graph','layers'];
  const api={kinds,stepCount:(_kind,spec)=>Math.max(1,spec.steps?.length??0),render(){throw new Error('그림 정의가 필요하다.');}};
  root.BookFigures=api;
  if(typeof module!=='undefined'&&module.exports)module.exports=api;
})(typeof globalThis!=='undefined'?globalThis:this);
