import procees from "node:process";
import axios from "axios";

const flags = process.argv.slice(2);

const url = "https:your/firebase/url_message.json";
const data = {
    "message": flags[0],
    "ref": 1234567
}

async function main(){
    const response = await axios.put(url, data);
    console.log(response.status);
}

main();