#! /home/ping/.conda/envs/neuro/bin/python
from locust import HttpUser, task, between

class QuickstartUser(HttpUser):
    # 模拟用户请求间隔（如不设置间隔，直接发包则去掉这行）
    # wait_time = between(0.1, 0.5)

    @task
    def test_index(self):
        self.client.get("/index.html")
