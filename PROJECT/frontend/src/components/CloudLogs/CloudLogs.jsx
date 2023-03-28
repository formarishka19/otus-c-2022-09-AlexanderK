import React, { useState, useRef, useCallback } from "react";
function CloudLogs() {

    const [isPaused, setIsPaused] = useState(true);
    const ws = useRef(null);
    const [data, setData] = useState([]);
    const [message, setMessage] = useState([]);
    const [status, setStatus] = useState("");
    const [server, setServer] = useState('ws://51.250.17.63:33223');
    const [userWord, setUserWord] = useState('');
    const [error, setError] = useState(false);

    const titleStyles = {
        'textAlign': "center",
        'margin': '0 0 10px',
        'padding': '0',
        'fontSize': '20px',
        'backgroundColor': 'rgb(13, 110, 253, .2)'
    }
    const appletStyle = {
        'border': '1px solid #0d6efd', 'padding': '0', 'borderRadius': '10px', 'position': "relative", 'boxSizing': "border-box",
        //'boxShadow': '10px 0 10px -10px rgba(0, 0, 0, .5)'
        'WebkitBoxSizing': 'border-box', 'overflowY': 'auto', 'textAlign': 'left',  
        'height': 'calc(70vh - 130px)',

        'height': 'calc(var(--vh, 1vh) * 70 - 135px)'

    }
    const connectSocket = (e) => {
        e.preventDefault()
        ws.current = new WebSocket(`${server}`); // 
        ws.current.onopen = () => { setStatus("Соединение открыто");setError(false) }
       
        ws.current.onclose = () => {setStatus("Соединение закрыто") }
        ws.current.onerror = (er) => {setError(true)}

        setIsPaused(!isPaused);
        gettingData();
    }
    const gettingData = useCallback(() => {
        if (!ws.current) return;
        let applet = document.getElementById('endDocs')
        ws.current.onmessage = e => {
            if (!isPaused) return;
            message.push(e.data);
            //let arr = message.length > 100 ? message.splice(1) : message.slice()
            let arr = message.slice();
            setData(arr);
            //applet.scrollTo(0, 0);
            //applet.scrollIntoView(false);
            applet.scrollIntoView({ block: "center", inline: "center" });
        };

    }, [isPaused]);

    const sendMessage = (word, e) => {

       
        if (ws.current) switch (word) {
            case ('stop'): ws.current.send(`stop`); break
            case (''): ws.current.send(`tail`); break
            case ('test'): ws.current.send(`tail:test`); break
            default: ws.current.send(`tail:${word} ${word}`)
        }
    }
    const stopConnect = (e) => {
     
        ws.current.close();
        setIsPaused(!isPaused)
    }

    const deleteMessages = () => {
        setData(null)
        setMessage(null)
    }
    return (

    
            <div style={{display: 'flex', flexDirection:'column', width: '100%', }}>
                {/* <h3 style={{fontSize: '40px', margin: '20px auto 10px'}}>Logsend</h3>

            <div style={{height: '1px', width: '100%', borderBottom: '0.5px solid grey'}}></div> */}

            <div style={{ display: 'flex', alignItems: 'flex-end', marginTop: '20px' }}>
                
                    <div style={{display: 'flex', flexDirection:'column', alignItems:'center', justifyContent:'flex-end'}}>
                        <label style={{fontSize: '18px', marginBottom: '5px'}}>Адрес подключения</label>
                        <input   type="text" value={server} style={{width: '300px', borderRadius: '8px', border: "1px solid green", height: '30px', padding: '0 0 0 5px', color: 'grey'}} onChange={(e) => setServer(e.target.value)}
                        />

                    </div>

             
                    <button  size="sm" 
                    
                    style={isPaused ? { fontSize: '12px', cursor:'pointer', width: '138px', marginLeft: '10px', padding: 0, height: '30px', background: 'none', borderRadius: '8px', border: "1px solid green" }:{ fontSize: '12px', cursor:'pointer', width: '138px', marginLeft: '10px', padding: 0, height: '30px', background: 'none', borderRadius: '8px', border: "1px solid red" } }
                        onClick={(e) => { isPaused ? connectSocket(e) : stopConnect(e) }}

                    >{isPaused ? 'Открыть соединение' : 'Остановить соединение'}</button>

               </div>


            <div style={{ display: 'flex', alignItems: 'flex-end', gap: '10px', margin: '15px 0', position: 'sticky', top: 0, zIndex: 1, minHeight: '2em', backgroundColor: 'white' }}>
                <div style={{display: 'flex', flexDirection:'column', alignItems:'center', justifyContent:'flex-end'}}>

                    <label style={{fontSize: '18px', marginBottom: '5px'}}size="sm">Запрос (optional grep)</label>
                    <input size="sm" type="text" value={userWord} style={{width: '300px', borderRadius: '8px', border: "1px solid green", height: '30px', color: 'grey', padding: '0 0 0 5px', cursor:'pointer'}}
                        onChange={(e) => setUserWord(e.target.value)}
                    />
                </div>
      
            
                    {<button variant="outline-success" size="sm" style={{ fontSize: '12px', width: '100px',  padding: 0, height: '30px', background: 'none', borderRadius: '8px', border: "1px solid green",cursor:'pointer' }}
                        onClick={(e) => { sendMessage(userWord,e) }}

                    >tail</button>}

          
              
                    {<button variant="outline-success" size="sm" style={{ fontSize: '12px', width: '100px',  padding: 0, height: '30px', background: 'none', borderRadius: '8px', border: "1px solid red", cursor:'pointer' }}
                        onClick={() => { sendMessage('stop') }}

                    >Стоп</button>}
<button variant="outline-danger" size="sm" style={{ fontSize: '12px', width: '100px',  padding: 0, height: '30px', background: 'none', borderRadius: '8px', border: "1px solid red" }}
                        onClick={() => { deleteMessages() }}

                    >Очистить</button>
               
            </div>

            <div className="mb-3 " style={{ minHeight: '600px', margin: '0 auto', width: '100%' }}>
                <div  style={appletStyle} >
                    <h4 style={titleStyles}>Ответ сервера</h4>
                    {error && <p style={{ paddingLeft: "5px", color: "red" }}>Ошибка подключения</p>}
                    <p style={{ paddingLeft: "5px" }}>{status}</p>
                    {data?.map((item, index) => item.includes('~') ? <p style={{ paddingLeft: "5px", color: 'red', fontWeight: 800 }} key={item + index}>{item.slice(2)}</p> :
                        <p style={{ paddingLeft: "5px" }} key={item + index} id='color'>
                            {item}</p>)}
                    <div id='endDocs'></div>
                </div>
            </div>
        </div>



    );
}
export default CloudLogs;