import app as dash


def test_parse_status():
    m = dash.parse_line("ST slots=1010 free=2 gate=OPEN\r\n")
    assert m == {"type": "status", "slots": [True, False, True, False],
                 "free": 2, "gate": "OPEN"}


def test_parse_full_and_junk():
    assert dash.parse_line("EV full") == {"type": "full"}
    assert dash.parse_line("# parking ready") is None
    assert dash.parse_line("ST slots=10x0 free=2 gate=OPEN") is None
    assert dash.parse_line("") is None


def test_state_and_api():
    state = dash.LotState()
    for line in ["# parking ready",
                 "ST slots=0000 free=4 gate=CLOSED",
                 "ST slots=1111 free=0 gate=CLOSED",
                 "EV full"]:
        state.feed(line)

    client = dash.create_app(state).test_client()
    data = client.get("/api/status").get_json()
    assert data["free"] == 0
    assert data["slots"] == [True] * 4
    assert data["full_count"] == 1
    assert len(data["log"]) == 3   # comment line not logged

    page = client.get("/")
    assert page.status_code == 200
    assert b"Free slots" in page.data


def test_page_before_any_data():
    client = dash.create_app(dash.LotState()).test_client()
    assert b"Waiting for data" in client.get("/").data
