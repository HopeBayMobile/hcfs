from django.db import models


class Work(models.Model):
    work_name = models.CharField(max_length=255, unique=True)
    current_form = models.CharField(max_length=255)


class Step(models.Model):
    wizard = models.ForeignKey(Work)
    form = models.CharField(max_length=255)
    data = models.TextField()
    task_id = models.CharField(max_length=255, null=True)
